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
#include <sys/timerfd.h>
#include<queue>


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
    int c_socket_pos;
    int c_timer_pos;
    std::queue<int> timers_queue;
    std::queue<Command> commands_queue;
};

int main()
{
    int listener;// слушающий сокет
    int sock;//сокет буфер?
    int bytes_read; // кол-во считанных байт командой recv
    unsigned short num_connections = 1;//кол-во подключений, всегда равени минимум 1, т.к. в общее кол-во включен слушающий сокет
    unsigned int num_unind_user = 1; //для нумерации неопознанных клиентов


    std::unordered_map<std::string, ClientInfo> clients;
    std::unordered_map<int, std::string> sock_x_name;

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
        ready = poll(pfds, num_connections*2-1, -1);
        
        printf("About to poll()\n");

        if (ready == -1){
            perror("poll");
            exit(2);
        }
        else if(pfds[0].revents & POLLIN){

            std::cout << "listener" << std::endl;
            sock = accept(listener, NULL, NULL);

            num_connections++;

            pfds = (pollfd*)realloc(pfds, sizeof(pollfd)*num_connections*2-1);
            pfds[num_connections*2-3].fd = sock;
            pfds[num_connections*2-3].events = POLLIN | POLLRDHUP;
            pfds[num_connections*2-2].fd = -1;
            pfds[num_connections*2-2].events = POLLIN;
            pfds[0].revents = 0;

            clients["unind_user_"+std::to_string(num_unind_user)].c_socket_pos = num_connections*2-3;

            clients["unind_user_"+std::to_string(num_unind_user)].c_timer_pos = num_connections*2-2;

            sock_x_name[sock] = "unind_user_"+std::to_string(num_unind_user);


            num_unind_user++;
        }
        else{
            
            for (const auto& [client_src_name, client_data] : clients){
                    printf("  fd=%s; events: %s%s%s%s\n", client_src_name.c_str(),
                    (pfds[client_data.c_socket_pos].revents & POLLHUP) ? "POLLHUP " : "",
                    (pfds[client_data.c_socket_pos].revents & POLLRDHUP)  ? "POLLRDHUP "  : "",
                    (pfds[client_data.c_socket_pos].revents  & POLLIN)  ? "POLLIN "  : "",
                    (pfds[client_data.c_socket_pos].revents  & POLLERR) ? "POLLERR " : "");

                    if(pfds[client_data.c_socket_pos].revents & POLLRDHUP){
                        std::cout << "User "<< client_src_name << " disconnected" << std:: endl;

                        close(pfds[client_data.c_socket_pos].fd);
                        pfds[client_data.c_socket_pos].revents = 0;

                        close(pfds[client_data.c_timer_pos].fd);
                        pfds[client_data.c_timer_pos].revents = 0;

                        for(int i = 0; i < num_connections*2-1; i++){
                            std::cout<<pfds[i].fd<< " ";
                        }
                        std::cout << std::endl;

                        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_socket_pos = client_data.c_socket_pos;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_timer_pos = client_data.c_timer_pos;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].timers_queue = client_data.timers_queue;

                        sock_x_name.erase(pfds[num_connections*2-3].fd);

                        pfds[client_data.c_socket_pos] = pfds[num_connections*2-3];
                        pfds[client_data.c_timer_pos] = pfds[num_connections*2-2];
                        
                        clients.erase(client_src_name);

                        
                        num_connections--;

                        for(int i = 0; i < num_connections*2-1; i++){
                            std::cout<<pfds[i].fd<< " ";
                        }
                        std::cout << std::endl;

                        pfds = (pollfd*)realloc(pfds, sizeof(pollfd)*num_connections*2-1);

                        break;
                    }
                    else if(pfds[client_data.c_socket_pos].revents & POLLIN){
                        std::cout << client_src_name << std::endl;

                        data = (Command*)malloc(24); // для приема сообщения
                    
                        bytes_read = recv(pfds[client_data.c_socket_pos].fd, data, 24, 0); // принимаем заголовок

                        data = (Command*)realloc(data, data->len);// выделяем память под сообщение

                        bytes_read = recv(pfds[client_data.c_socket_pos].fd, data->message, data->len-24, 0);// считываем сообщение

                        std::cout << "TYPE: " << data->type << std::endl;
                        std::cout << "SRC: " << data->src_username << std::endl;
                        std::cout << "DST: " << data->dst_username << std::endl;
                        std::cout << "MESSAGE: " << data->message << std::endl;
                        

                        if(clients.count(data->dst_username) && data->type == 0){
                            int tmp_timer = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
                             if (timerfd_settime(tmp_timer, 0, &timer_settings, 0) == -1){
                                perror("poll");
                                exit(2);
                            }
                            std::cout <<client_src_name << " -> " << data->dst_username << ": message sended, bytes " << send(pfds[clients[data->dst_username].c_socket_pos].fd, data, data->len, 0) << std::endl;
                            clients[data->dst_username].timers_queue.push(tmp_timer);
                            if(pfds[clients[data->dst_username].c_timer_pos].fd == -1) {
                               pfds[clients[data->dst_username].c_timer_pos].fd = tmp_timer;
                            }
                           
                        }

                        else if(clients.count(data->dst_username) && data->type == 1){
                            std::cout <<client_src_name << " -> " << data->dst_username << ": message sended, bytes " << send(pfds[clients[data->dst_username].c_socket_pos].fd, data, data->len, 0) << std::endl;
                            
                            close(clients[data->src_username].timers_queue.front());
                            pfds[clients[data->src_username].c_timer_pos].revents = 0;

                            clients[data->src_username].timers_queue.pop();
                            
                            if(!clients[data->src_username].timers_queue.empty()){
                                pfds[clients[data->src_username].c_timer_pos].fd = clients[data->src_username].timers_queue.front();
                            }
                            else{
                                pfds[clients[data->src_username].c_timer_pos].fd = -1;
                            }
                        }

                        else if(!(clients.count(data->dst_username)) && data->type == 0){
                            std::cout <<client_src_name << " -> " << data->dst_username << ": the recipient is offline"<< std::endl;

                            for(int i = 0; i < 8; i++){
                                SWAP(char, data->dst_username[i], data->src_username[i]); //для отправки ответа клиенту отправителю от лица неподключенного клиента получателя 
                            }

                            data = (Command*)realloc(data, 28);//на заголовок и сообщение/ответ "300"

                            memcpy(data->message, "300", 4);
                            data->len = 28;
                            data->type = 1;
                            send(pfds[clients[client_src_name].c_socket_pos].fd, data, data->len, 0);
                        
                            for(int i = 0; i < 8; i++){
                                SWAP(char, data->dst_username[i], data->src_username[i]);
                            }
                        }

                    
                        
                        if(data->src_username != client_src_name){
                            pfds[clients[data->src_username].c_socket_pos].revents = 0;
                            sock_x_name.erase(pfds[client_data.c_socket_pos].fd);
                            
                            clients[data->src_username].c_socket_pos = client_data.c_socket_pos;
                            clients[data->src_username].c_timer_pos = client_data.c_timer_pos;

                            sock_x_name[pfds[client_data.c_socket_pos].fd] = data->src_username;
                            clients.erase(client_src_name);

                            delete data;
                            data = nullptr;
                            break;
                        }
                        else{

                            delete data;
                            data = nullptr;
                            //pfds[client_data.c_socket_pos].revents = 0;
                        }
                    }

                    else if(pfds[client_data.c_timer_pos].revents & POLLIN){
                        std::cout << "User "<< client_src_name << " disconnected" << std:: endl;

                        close(pfds[client_data.c_socket_pos].fd);
                        pfds[client_data.c_socket_pos].revents = 0;

                        close(pfds[client_data.c_timer_pos].fd);
                        pfds[client_data.c_timer_pos].revents = 0;
                        std::cout<<pfds[num_connections*2-3].fd<<std::endl;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_socket_pos = client_data.c_socket_pos;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_timer_pos = client_data.c_timer_pos;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].timers_queue = client_data.timers_queue;

                        sock_x_name.erase(pfds[num_connections*2-3].fd);

                        pfds[client_data.c_socket_pos] = pfds[num_connections*2-3];
                        pfds[client_data.c_timer_pos] = pfds[num_connections*2-2];
                        
                        clients.erase(client_src_name);
                        
                        num_connections--;

                        pfds = (pollfd*)realloc(pfds, sizeof(pollfd)*num_connections*2-1);

                        break;
                    }
            }
        }
        std::cout << "end\n" << std::endl;
    }
    std::cout << "ogg" << std::endl;
    return 0;
}