//Запуск сервера: в командной строке указать сначала свой айпи, затем порт

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

#define USERNAME_MAX_LEN 8
#define HEADER_LEN 24
#define MESSAGE_MAX_LEN 1000

struct Command{
    unsigned short len;
    unsigned short type;
    unsigned int message_ID;
    char src_username[USERNAME_MAX_LEN];
    char dst_username[USERNAME_MAX_LEN];
    char message[];
};

struct ClientInfo{
    int c_socket_pos;
    int c_timer_pos;
    std::queue<int> timers_queue;
    std::queue<Command*> commands_queue;
};

std::unordered_map<std::string, ClientInfo> clients;
std::unordered_map<int, std::string> sock_x_name;
struct pollfd *pfds = (pollfd*)malloc(sizeof(struct pollfd));
struct itimerspec timer_settings;
struct sockaddr_in addr;
unsigned short num_connections = 1;//кол-во подключений, всегда равени минимум 1, т.к. в общее кол-во включен слушающий сокет
unsigned int num_unind_user = 1;


void Usage(char *program_name){
    std::cout << "Usage: " << program_name << " <IP> <Port>" << std::endl;
}


void save(std::string filename, Command *p){
    std::ofstream f1(filename+ ".bin", std::ios::binary | std::ios::app);
    f1.write((char*)p, p->len);
    f1.close();
}

void load_and_send(const std::string recipiter,const pollfd* pfds, std::unordered_map<std::string, ClientInfo> clients)
{
    Command *data;
    
    std::ifstream f2(recipiter+".bin", std::ios::binary | std::ios::in);
    
    while(f2.peek() != EOF){
        data = (Command*)malloc(HEADER_LEN);
        f2.read((char*)data, HEADER_LEN);
        data = (Command*)realloc(data, data->len);
        f2.read((char*)data->message, data->len-HEADER_LEN);
        data->type = 2;
        std::cout <<data->src_username << " -> " << data->dst_username << ": message sended, bytes " << send(pfds[clients[data->dst_username].c_socket_pos].fd, data, data->len, 0) << std::endl;
    }

   
    std::cout << "File is empty" << std::endl;
    std::cout << "All messages sent" << std::endl;
    f2.close();
    remove((recipiter+".bin").c_str());
}

void Event_AcceptNewClient(int listener){

    std::cout << "listener" << std::endl;
    int sock = accept(listener, NULL, NULL);

    num_connections++;
    //при подключении одного клиента количество дескрипторов, которые слушает poll, увеличивается на 2, т.е. +1 сокет и +1 таймер
    pfds = (pollfd*)realloc(pfds, sizeof(pollfd)*(num_connections*2-1));

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

void Event_DisconnectClient(std::string client_src_name, ClientInfo &client_data){
    std::cout << "User "<< client_src_name << " disconnected" << std:: endl;

        close(pfds[client_data.c_socket_pos].fd);
        pfds[client_data.c_socket_pos].revents = 0;

        close(pfds[client_data.c_timer_pos].fd);
        pfds[client_data.c_timer_pos].revents = 0;

        while(!client_data.commands_queue.empty()){
            save(client_src_name, client_data.commands_queue.front());
            client_data.commands_queue.pop();
        }

        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_socket_pos = client_data.c_socket_pos;
        clients[sock_x_name[pfds[num_connections*2-3].fd]].c_timer_pos = client_data.c_timer_pos;

        sock_x_name.erase(pfds[num_connections*2-3].fd);

        pfds[client_data.c_socket_pos] = pfds[num_connections*2-3];
        pfds[client_data.c_timer_pos] = pfds[num_connections*2-2];
        
        clients.erase(client_src_name);

        num_connections--;
        //при отключении одного клиента количество дескрипторов, которые слушает poll, уменьшается на 2, т.е. -1 сокет и -1 таймер
        pfds = (pollfd*)realloc(pfds, sizeof(pollfd)*(num_connections*2-1));
}

void Event_DisconnectAll( unsigned short& num_connections){
    while(!clients.empty()){
        for ( auto& [client_src_name, client_data] : clients){
            Event_DisconnectClient(client_src_name, client_data);
            break;
        }
    }
}

void AcceptCommand(Command *&data, ClientInfo &client_data){

    data = (Command*)malloc(HEADER_LEN); // для приема заголовка

    int bytes_read = recv(pfds[client_data.c_socket_pos].fd, data, HEADER_LEN, 0); // принимаем заголовок

    std::cout << "TYPE: " << data->type << std::endl;
    std::cout << "SRC: " << data->src_username << std::endl;
    std::cout << "DST: " << data->dst_username << std::endl;

    data = (Command*)realloc(data, data->len);// выделяем память под сообщение

    bytes_read = recv(pfds[client_data.c_socket_pos].fd, data->message, data->len-HEADER_LEN, 0);// считываем сообщение

    std::cout << "MESSAGE: " << data->message << std::endl;
}

void PrintInfoAboutPoll(const std::string client_src_name, const ClientInfo &client_data){
    printf("  fd=%s; events: %s%s%s%s\n", client_src_name.c_str(),
        (pfds[client_data.c_socket_pos].revents & POLLHUP) ? "POLLHUP " : "",
        (pfds[client_data.c_socket_pos].revents & POLLRDHUP)  ? "POLLRDHUP "  : "",
        (pfds[client_data.c_socket_pos].revents  & POLLIN)  ? "POLLIN "  : "",
        (pfds[client_data.c_socket_pos].revents  & POLLERR) ? "POLLERR " : "");
}

void ForwardMessageOnline(std::string client_src_name,  Command *data){
    std::cout <<client_src_name << " -> " << data->dst_username << ": message sended, bytes " << send(pfds[clients[data->dst_username].c_socket_pos].fd, data, data->len, 0) << std::endl;

    int tmp_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd_settime(tmp_timer, 0, &timer_settings, 0) == -1){
        perror("Timer");
        exit(1);
    }
    clients[data->dst_username].timers_queue.push(tmp_timer);
    if(pfds[clients[data->dst_username].c_timer_pos].fd == -1) {
        pfds[clients[data->dst_username].c_timer_pos].fd = tmp_timer;
    }
    
    Command *tmp = (Command*)malloc(data->len);
    memcpy(tmp,data,data->len);
    clients[data->dst_username].commands_queue.push(tmp);
    tmp = nullptr;
}

void ForwardAnswerOnline(std::string client_src_name, Command *answer){
    std::cout <<client_src_name << " -> " << answer->dst_username << ": message sended, bytes " << send(pfds[clients[answer->dst_username].c_socket_pos].fd, answer, answer->len, 0) << std::endl;
                            
    close(clients[answer->src_username].timers_queue.front());
    pfds[clients[answer->src_username].c_timer_pos].revents = 0;

    clients[answer->src_username].timers_queue.pop();
    
    if(!clients[answer->src_username].timers_queue.empty()){
        pfds[clients[answer->src_username].c_timer_pos].fd = clients[answer->src_username].timers_queue.front();
    }
    else{
        pfds[clients[answer->src_username].c_timer_pos].fd = -1;
    }

    clients[answer->src_username].commands_queue.pop();
}

void SaveMessageOffline(const std::string client_src_name, Command *data){
    std::cout <<client_src_name << " -> " << data->dst_username << ": the recipient is offline"<< std::endl;

    save(data->dst_username, data);

    std::swap(data->dst_username, data->src_username); //для отправки ответа клиенту отправителю от лица неподключенного клиента получателя 
    
    data = (Command*)realloc(data, HEADER_LEN+3);//на заголовок и сообщение/ответ "300"

    memcpy(data->message, "300", 3);
    data->len = HEADER_LEN+3;
    data->type = 1;
    send(pfds[clients[client_src_name].c_socket_pos].fd, data, data->len, 0);

    std::swap(data->dst_username, data->src_username);

}

void IdentifyUser(const std::string client_src_name, ClientInfo &client_data, Command *data){
    pfds[clients[data->src_username].c_socket_pos].revents = 0;
    sock_x_name.erase(pfds[client_data.c_socket_pos].fd);
    
    clients[data->src_username].c_socket_pos = client_data.c_socket_pos;
    clients[data->src_username].c_timer_pos = client_data.c_timer_pos;

    sock_x_name[pfds[client_data.c_socket_pos].fd] = data->src_username;
    clients.erase(client_src_name);

    
    std::cout<<" LOAD " << data->src_username << std::endl;
    load_and_send(data->src_username,pfds,clients);
}

void Event_ClientProcessing( Command *data){
    for ( auto& [client_src_name, client_data] : clients){
        PrintInfoAboutPoll(client_src_name, client_data);

        //Клиент отключился
        if(pfds[client_data.c_socket_pos].revents & POLLRDHUP || pfds[client_data.c_socket_pos].revents & POLLHUP){
            Event_DisconnectClient(client_src_name, client_data);
            break;
        }

        //Клиент что-то отправил
        else if(pfds[client_data.c_socket_pos].revents & POLLIN){

            AcceptCommand(data, client_data);
            
            //Пришло новое сообщение от отправителя к получателю, проверка на подключение получателя
            if(clients.count(data->dst_username) && data->type == 0){
                ForwardMessageOnline(client_src_name, data);
            }

            //Пришел ответ от получателя, отправитель подключен к серверу
            else if(clients.count(data->dst_username) && data->type == 1){
                ForwardAnswerOnline(client_src_name, data);
            }

            //пришло новое сообщение от отправителя, получатель не подключен к серверу.
            else if(!(clients.count(data->dst_username)) && data->type == 0){
                SaveMessageOffline(client_src_name,data);   
            }

            //пришел ответ от получателя, но отправитель не подключен к серверу.
            else{
                clients[data->src_username].commands_queue.pop();
            }

            //Неидентифицированный пользователь отправил сообщение, тееперь он идентифицирован
            if(data->src_username != client_src_name){
                IdentifyUser(client_src_name, client_data, data);
            }

            free(data);
            data = nullptr;
            break;
        }

        //Клиент не ответил на сообщение с type = 0 за 3 секунды, поэтому считаем его отключенным
        else if(pfds[client_data.c_timer_pos].revents & POLLIN){
            Event_DisconnectClient(client_src_name, client_data);
            break;
        }
    }
    
}


int main(int argc, char* argv[])
{
    if(argc != 3 ){
        Usage(argv[0]);
        exit(3);
    }

    int listener;
    char* p_end;
    struct Command *data;

    timer_settings.it_value.tv_sec = 3;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if(listener < 0)
    {
        perror("socket");
        exit(1);
    }

    struct in_addr inp;
    long port = strtol(argv[2], &p_end, 10);

    if(*p_end!='\0' || port<0 || port>65535 || !inet_aton( argv[1] , &inp)/*ip*/){
        perror("Wrong enter, you need to enter separated by spase your IP and Port");
        exit(3);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inp.s_addr;

    if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    pfds[0].fd = listener;
    pfds[0].events = POLLIN ;
    int ready;

    listen(listener, 1);
    
    std::cout << "Server is runnig" << std::endl;
    
    while(1)
    {
        //Маленькая справка по массиву полов:
        //на 0 месте всегда стоит дескриптор слушающего сокета
        //далее на нечетных местах(1, 3, 5...) стоят дескрипторы сокетов подключенных к серверу клиентов
        //на четных местах(2, 4, 6...) стоят дескрипторы таймеров принятых клиентов
        //в итоге получается пара дескрипторов [сокет, таймер] для одного клиента в общем массиве дескрипторов. 
        ready = poll(pfds, num_connections*2-1, 3600000);
        
        printf("About to poll:\n");
        if (ready == -1 && errno != EINTR){
            perror("Poll");
            exit(1);
        }
        //Если сервер находился в простое 1 час, отключить всех клиентов и выключить сервер
        else if(ready == 0){
            Event_DisconnectAll(num_connections);
            break;
        }
        //Сработал дескриптор слушающего сокета
        else if(pfds[0].revents & POLLIN){
            Event_AcceptNewClient(listener);
        }
        else{
            Event_ClientProcessing(data);
        }
        std::cout << "end\n\n" << std::endl;
    }
    free(pfds);
    std::cout << "server is disabled"<<std::endl;
    return 0;
}