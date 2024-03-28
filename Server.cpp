#include<iostream>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<poll.h>
#include<map>
#include <unordered_map>
#include<string.h>
#include <fstream>


#define SWAP(type, a, b) type tmp = a; a = b; b = tmp;

struct Command{
    unsigned short len;
    unsigned short type;
    unsigned int message_ID;
    char src_username[8];
    char dst_username[8];
    char message[];
};

struct ClientInfo{
    short *revents;
    int *c_socket;
    int *c_timer;
};

int main()
{
    int listener;// слушающий сокет
    int sock;//сокет буфер?
    int bytes_read; // кол-во считанных байт командой recv
    unsigned short num_connections = 1;//кол-во подключенных клиентов, всегда равени минимум 1, т.к. в общее кол-во подключений включаем слушающий сокет
    unsigned int num_unind_user = 1; //для нумерации неопознанных клиентов


    std::unordered_map<std::string, ClientInfo> clients;

    struct sockaddr_in addr;
    struct pollfd *pfds = (pollfd*)malloc(sizeof(struct pollfd)*1);
    struct itimerspec timer_settings;
    struct Command *data;// буфер приема и последующей обработки команд
    timer_settings.it_value.tv_sec = 3;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if(listener < 0)
    {
        perror("socket");
        exit(1);
    }
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6666);
    addr.sin_addr.s_addr = htonl((192<<24)+(168<<16)+(10<<8)+126);
    if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(2);
    }

    pfds[0].fd = listener;
    pfds[0].events = POLLIN ;
    int ready;


    listen(listener, 1);
    
    while(1)
    {
        ready = poll(pfds, num_connections, -1);
        
        printf("About to poll()\n");

        if (ready == -1){
            perror("poll");
            exit(2);
        }
        else if(pfds[0].revents & POLLIN){

            std::cout << "listener" << std::endl;
            sock = accept(listener, NULL, NULL);

            num_connections++;
            pfds = (pollfd*)realloc(pfds, sizeof(pollfd)*num_connections);
            pfds[num_connections-1].fd = sock;
            pfds[num_connections-1].events = POLLIN | POLLRDHUP;
            pfds[0].revents = 0;

            clients["unind_user"+std::to_string(num_unind_user)].c_socket = &pfds[num_connections-1].fd;
            clients["unind_user"+std::to_string(num_unind_user)].revents = &pfds[num_connections-1].revents;
            num_unind_user++;
        }
        else{
            
            for (const auto& [client_src_name, client_data] : clients){
                    printf("  fd=%s; events: %s%s%s%s\n", client_src_name.c_str(),
                    (*(client_data.revents) & POLLHUP) ? "POLLHUP " : "",
                    (*(client_data.revents) & POLLRDHUP)  ? "POLLRDHUP "  : "",
                    (*(client_data.revents) & POLLIN)  ? "POLLIN "  : "",
                    (*(client_data.revents) & POLLERR) ? "POLLERR " : "");

                    if(*(client_data.revents) & POLLRDHUP){
                        std::cout << "User "<< client_src_name << " disconnected" << std:: endl;
                        num_connections--;
                        close(*(client_data.c_socket));
                        *(client_data.revents) = 0;
                        
                        clients.erase(client_src_name);
                        break;
                    }
                    else if(*(client_data.revents) & POLLIN){
                        std::cout << client_src_name << std::endl;

                        data = (Command*)malloc(24); // для приема сообщения
                    
                        bytes_read = recv(*(client_data.c_socket), data, 24, 0); // принимаем заголовок

                        data = (Command*)realloc(data, data->len);// выделяем память под сообщение

                        bytes_read = recv(*(client_data.c_socket), data->message, data->len-24, 0);// считываем сообщение

                        // std::cout << bytes_read << std::endl;
                        // std::cout << data->len << std::endl;
                        // std::cout << data->type << std::endl;
                        // std::cout << data->message_ID << std::endl;
                        // std::cout << data->src_username << std::endl;
                        // std::cout << data->dst_username << std::endl;
                        // std::cout << data->message << std::endl;


                        if(clients.count(data->dst_username)){
                            std::cout <<client_src_name << " -> " << data->dst_username << ": message sended, bytes " << send(*(clients[data->dst_username].c_socket), data, data->len, 0) << std::endl;
                        }
                        else{
                            std::cout <<client_src_name << " -> " << data->dst_username << ": the recipient is offline"<< std::endl;

                            for(int i = 0; i < 8; i++){
                                SWAP(char, data->dst_username[i], data->src_username[i]); //для отправки ответа клиенту отправителю от лица неподключенного клиента получателя 
                            }

                            data = (Command*)realloc(data, 28);//на заголовок и сообщение/ответ "300"

                            memcpy(data->message, "300", 4);
                            data->len = 28;
                            send(*(client_data.c_socket), data, data->len, 0);
                        
                            for(int i = 0; i < 8; i++){
                                SWAP(char, data->dst_username[i], data->src_username[i]);
                            }
                        }
                        
                        if(data->src_username != client_src_name){
                            *(client_data.revents) = 0;
                            clients[data->src_username].c_socket = client_data.c_socket;
                            clients[data->src_username].c_timer = client_data.c_timer;
                            clients[data->src_username].revents = client_data.revents;
                            clients.erase(client_src_name);

                            delete data;
                            data = nullptr;
                            break;
                        }
                        else{

                            delete data;
                            data = nullptr;
                            *(client_data.revents) = 0;
                        }
                    }
            }
            // for(int i = 1; i < num_connections; i++){
            //     printf("  fd=%d; events: %s%s%s%s\n", pfds[i].fd,
                
            //     (pfds[i].revents & POLLHUP) ? "POLLHUP " : "",
            //     (pfds[i].revents & POLLRDHUP)  ? "POLLRDHUP "  : "",
            //     (pfds[i].revents & POLLIN)  ? "POLLIN "  : "",
            //     (pfds[i].revents & POLLERR) ? "POLLERR " : "");

            //     if(pfds[i].revents & POLLRDHUP){
            //         std::cout << i << "Отключился" << std:: endl;
            //         pfds[i].revents = 0;
            //     }
            //     else if(pfds[i].revents & POLLIN){
            //         std::cout << "sock" << i << std::endl;
            //         Command *data = (Command*)malloc(24);
                
            //         bytes_read = recv(pfds[i].fd, data, 24, 0);

            //         std::cout << (pfds[i].revents & POLLHUP )  << std::endl;
            //         std::cout << (pfds[i].revents & POLLRDHUP)<< std::endl;

            //         data = (Command*)realloc(data, data->len);

            //         bytes_read = recv(pfds[i].fd, data->message, data->len-24, 0);

            //         std::cout << bytes_read << std::endl;
            //         std::cout << data->len << std::endl;
            //         std::cout << data->type << std::endl;
            //         std::cout << data->message_ID << std::endl;
            //         std::cout << data->src_username << std::endl;
            //         std::cout << data->dst_username << std::endl;
            //         std::cout << data->message << std::endl;
            //         delete data;
            //         data = nullptr;
            //         pfds[i].revents = 0;
            //     }
            // }
        }

        std::cout << "end\n" << std::endl;

        // printf("Ready: %d\n", ready);

        // if(pfds[0].revents & POLLIN){
        //     std::cout << "ZAEBIS" << std::endl;
        // }

        /* Deal with array returned by poll() */

        // for (int j = 0; j < num_connections; j++) {
        //     char buf[10];

        //     if (pfds[j].revents != 0) {
        //         printf("  fd=%d; events: %s%s%s\n", pfds[j].fd,
        //                 (pfds[j].revents & POLLIN)  ? "POLLIN "  : "",
        //                 (pfds[j].revents & POLLHUP) ? "POLLHUP " : "",
        //                 (pfds[j].revents & POLLERR) ? "POLLERR " : "");

        //         if (pfds[j].revents & POLLIN) {
        //             ssize_t s = read(pfds[j].fd, buf, sizeof(buf));
        //             if (s == -1)
        //                 errExit("read");
        //             printf("    read %zd bytes: %.*s\n",
        //                     s, (int) s, buf);
        //         } else {                /* POLLERR | POLLHUP */
        //             printf("    closing fd %d\n", pfds[j].fd);
        //             if (close(pfds[j].fd) == -1)
        //                 errExit("close");
        //             num_open_fds--;
        //         }
        //     }
        // }



        
        
        
        // if(sock < 0)
        // {
        //     perror("accept");
        //     exit(3);
        // }
        // else{
        //     std::cout << "sock is opened" << std::endl;
        // }
     
        // Command *data = (Command*)malloc(24);
    
        // bytes_read = recv(sock, data, 24, 0);

        // data = (Command*)realloc(data, data->len);

        // bytes_read = recv(sock, data->message, data->len-24, 0);

        // std::cout << bytes_read << std::endl;
        // std::cout << data->len << std::endl;
        // std::cout << data->type << std::endl;
        // std::cout << data->message_ID << std::endl;
        // std::cout << data->src_username << std::endl;
        // std::cout << data->dst_username << std::endl;
        // std::cout << data->message << std::endl;
        

        // close(sock);

        // delete data;
        // data = nullptr;
        
        // std::cout << "sock is closed" << std::endl;

    }
    std::cout << "ogg" << std::endl;
    return 0;
}