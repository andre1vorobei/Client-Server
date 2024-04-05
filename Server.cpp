#include<iostream>
#include<fstream>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <arpa/inet.h>
#include<unistd.h>
#include<poll.h>
#include<map>
#include <unordered_map>
#include<string.h>
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
    std::queue<Command*> commands_queue;
};

void save(std::string filename, Command *p){
    std::ofstream f1(filename+ ".bin", std::ios::binary | std::ios::app);
    f1.write((char*)p, p->len);
    f1.close();
}

//Чтение из файла и последующая отправка
void load_and_send(const std::string recipiter,const pollfd* pfds, std::unordered_map<std::string, ClientInfo> clients)
{
    
    Command *data;
    
    std::ifstream f2(recipiter+".bin", std::ios::binary | std::ios::in);
    
    while(f2.peek() != EOF){
        data = (Command*)malloc(24);
        f2.read((char*)data, 24);
        data = (Command*)realloc(data, data->len);
        f2.read((char*)data->message, data->len-24);
        data->type = 2;
        //Сделать отделюную ф-цию отправки сообщения
        std::cout <<data->src_username << " -> " << data->dst_username << ": message sended, bytes " << send(pfds[clients[data->dst_username].c_socket_pos].fd, data, data->len, 0) << std::endl;
    }

   
    std::cout << "File is empty" << std::endl;
    std::cout << "All messages sent" << std::endl;
    f2.close();
    remove((recipiter+".bin").c_str());
}



int main(int argc, char* argv[])
{

    if(argc != 3 ){
        std::cout << "wrong arguments" << std::endl;
        perror("enter");
        exit(4);
    }

    int listener;// слушающий сокет
    int sock;//сокет буфер?
    int bytes_read; // кол-во считанных байт командой recv
    unsigned short num_connections = 1;//кол-во подключений, всегда равени минимум 1, т.к. в общее кол-во включен слушающий сокет
    unsigned int num_unind_user = 1; //для нумерации неопознанных клиентов
    char* p_end;//для указатель на символ, где прервалось преобразование строкового типа в числовой, нужно для проверок ввода

    std::unordered_map<std::string, ClientInfo> clients;//Для обращения к информации киента по имени клиента
    std::unordered_map<int, std::string> sock_x_name;// Для получения имени клиента по сокету

    struct sockaddr_in addr;
    struct pollfd *pfds = (pollfd*)malloc(sizeof(struct pollfd)*1);// Все дескрипторы, которые будет слушать Poll
    struct Command *data;// буфер приема и последующей обработки команд
    struct itimerspec timer_settings;

    timer_settings.it_value.tv_sec = 3;// выставление общей настройки таймера на 3 секунды

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if(listener < 0)
    {
        perror("socket");
        exit(1);
    }
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr( argv[1] );
    addr.sin_port = htons(strtol(argv[2], &p_end, 10));

    if(strlen(p_end)!=0){
        perror("enter");
        exit(4);
    }
    
    if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(2);
    }

    pfds[0].fd = listener;
    pfds[0].events = POLLIN ;
    int ready;


    listen(listener, 3);
    
    std::cout << "Server is runnng" << std::endl;
    
    while(1)
    {
        ready = poll(pfds, num_connections*2-1, -1);
        
        printf("About to poll()\n");

        if (ready == -1){
            perror("poll");
            exit(3);
        }
        else if(pfds[0].revents & POLLIN){

            std::cout << "listener" << std::endl;
            sock = accept(listener, NULL, NULL);

            num_connections++;

            pfds = (pollfd*)realloc(pfds, sizeof(pollfd)*num_connections*2-1);
            pfds[num_connections*2-3].fd = sock;
            pfds[num_connections*2-3].events = POLLIN | POLLRDHUP | POLLHUP;
            pfds[num_connections*2-2].fd = -1;
            pfds[num_connections*2-2].events = POLLIN;
            pfds[0].revents = 0;

            clients["unind_user_"+std::to_string(num_unind_user)].c_socket_pos = num_connections*2-3;

            clients["unind_user_"+std::to_string(num_unind_user)].c_timer_pos = num_connections*2-2;

            sock_x_name[sock] = "unind_user_"+std::to_string(num_unind_user);


            num_unind_user++;
        }
        else{
            
            for ( auto& [client_src_name, client_data] : clients){
                    printf("  fd=%s; events: %s%s%s%s\n", client_src_name.c_str(),
                    (pfds[client_data.c_socket_pos].revents & POLLHUP) ? "POLLHUP " : "",
                    (pfds[client_data.c_socket_pos].revents & POLLRDHUP)  ? "POLLRDHUP "  : "",
                    (pfds[client_data.c_socket_pos].revents  & POLLIN)  ? "POLLIN "  : "",
                    (pfds[client_data.c_socket_pos].revents  & POLLERR) ? "POLLERR " : "");

                    if(pfds[client_data.c_socket_pos].revents & POLLRDHUP || pfds[client_data.c_socket_pos].revents & POLLHUP){
                        std::cout << "User "<< client_src_name << " disconnected" << std:: endl;

                        close(pfds[client_data.c_socket_pos].fd);
                        pfds[client_data.c_socket_pos].revents = 0;

                        close(pfds[client_data.c_timer_pos].fd);
                        pfds[client_data.c_timer_pos].revents = 0;

                        for(int i = 0; i < num_connections*2-1; i++){
                            std::cout<<pfds[i].fd<< " ";
                        }
                        std::cout << std::endl;

                        while(!client_data.commands_queue.empty()){
                            save(client_src_name, client_data.commands_queue.front());
                            client_data.commands_queue.pop();
                        }

                        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_socket_pos = client_data.c_socket_pos;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_timer_pos = client_data.c_timer_pos;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].timers_queue = client_data.timers_queue;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].commands_queue = client_data.commands_queue;

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
                        
                        //пришло новое сообщение от отправителя к получателю, проверка на подключение получателя
                        if(clients.count(data->dst_username) && data->type == 0){
                            std::cout <<client_src_name << " -> " << data->dst_username << ": message sended, bytes " << send(pfds[clients[data->dst_username].c_socket_pos].fd, data, data->len, 0) << std::endl;

                            int tmp_timer = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
                             if (timerfd_settime(tmp_timer, 0, &timer_settings, 0) == -1){
                                perror("poll");
                                exit(3);
                            }
                            
                            clients[data->dst_username].timers_queue.push(tmp_timer);
                            if(pfds[clients[data->dst_username].c_timer_pos].fd == -1) {
                               pfds[clients[data->dst_username].c_timer_pos].fd = tmp_timer;
                            }
                            
                            //test
                            //Добавление сообщения в очередь сообщений
                            Command *tmp = (Command*)malloc(data->len);
                            memcpy(tmp,data,data->len);
                            // tmp.message = new char[data->len-24];
                            clients[data->dst_username].commands_queue.push(tmp);
                            tmp = nullptr;
                           
                        }

                        //пришел ответ от получателя
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
                            //убираем собщение из очереди сообщений
                            clients[data->src_username].commands_queue.pop();
                        }

                        //пришло новое сообщение от отправителя, получатель не подключен к серверу.
                        else if(!(clients.count(data->dst_username)) && data->type == 0){
                            std::cout <<client_src_name << " -> " << data->dst_username << ": the recipient is offline"<< std::endl;

                            save(data->dst_username, data);

                            //ответ отправителю о том, что его сообщение не доставлено 
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

                        //пришел ответ от получателя, но отправитель не подключен к серверу.
                        else{
                            clients[data->src_username].commands_queue.pop();
                        }

                    
                        //Неидентифицированный пользователь отправил сообщение, тееперь он идентифицирован
                        if(data->src_username != client_src_name){
                            pfds[clients[data->src_username].c_socket_pos].revents = 0;
                            sock_x_name.erase(pfds[client_data.c_socket_pos].fd);
                            
                            clients[data->src_username].c_socket_pos = client_data.c_socket_pos;
                            clients[data->src_username].c_timer_pos = client_data.c_timer_pos;

                            sock_x_name[pfds[client_data.c_socket_pos].fd] = data->src_username;
                            clients.erase(client_src_name);

                            
                            std::cout<<" LOAD " << data->src_username << std::endl;
                            load_and_send(data->src_username,pfds,clients);
                            

                            delete data;
                            data = nullptr;
                            break;
                        }
                        else{

                            delete data;
                            data = nullptr;
                            break;
                        }
                    }

                    else if(pfds[client_data.c_timer_pos].revents & POLLIN){
                         std::cout << "User "<< client_src_name << " disconnected" << std:: endl;

                        close(pfds[client_data.c_socket_pos].fd);
                        pfds[client_data.c_socket_pos].revents = 0;

                        close(pfds[client_data.c_timer_pos].fd);
                        pfds[client_data.c_timer_pos].revents = 0;

                        for(int i = 0; i < num_connections*2-1; i++){
                            std::cout<<pfds[i].fd<< " ";
                        }
                        std::cout << std::endl;

                        while(!client_data.commands_queue.empty()){
                            save(client_src_name, client_data.commands_queue.front());
                            client_data.commands_queue.pop();
                        }

                        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_socket_pos = client_data.c_socket_pos;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_timer_pos = client_data.c_timer_pos;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].timers_queue = client_data.timers_queue;
                        clients[sock_x_name[pfds[num_connections*2-3].fd]].commands_queue = client_data.commands_queue;

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
            }
        }
        std::cout << "end\n" << std::endl;
    }
    std::cout << "ogg" << std::endl;
    return 0;
}