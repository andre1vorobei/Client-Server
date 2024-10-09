//Запуск сервера: в командной строке указать порт

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
#include <time.h> 

#define USERNAME_MAX_LEN 8
#define HEADER_LEN 24
#define MESSAGE_MAX_LEN 1000

struct Command{
    unsigned short len;
    unsigned short type;
    unsigned int message_id;
    char src_username[USERNAME_MAX_LEN];
    char dst_username[USERNAME_MAX_LEN];
    char message[];
};

struct ClientInfo{
    int c_socket_pos;
    int c_timer_pos;
    std::queue<int> timers_queue;
    std::queue<Command*> commands_queue;
    Command *current_command;
    int head_recv_bytes;
    int message_recv_bytes;
};

std::unordered_map<std::string, ClientInfo> clients;
std::unordered_map<int, std::string> sock_to_name;
struct pollfd *pfds = (pollfd*)malloc(sizeof(struct pollfd));
struct itimerspec timer_settings;
struct sockaddr_in addr;
unsigned short num_clients = 0;
unsigned int num_unind_user = 1;


void Usage(char *program_name){
    std::cout << "Usage: " << program_name << " <Port>" << std::endl;
}


void save(std::string filename, Command *p){
    std::ofstream f1(filename+ ".bin", std::ios::binary | std::ios::app);
    f1.write((char*)p, p->len);
    f1.close();
}

void load_and_send(const std::string recipiter)
{
    Command *recv_command;
    
    std::ifstream f2(recipiter+".bin", std::ios::binary | std::ios::in);

    if(f2.peek() == EOF){
        std::cout << "File is empty" << std::endl;
    }
    else{
        while(f2.peek() != EOF){
            recv_command = (Command*)malloc(HEADER_LEN);
            f2.read((char*)recv_command, HEADER_LEN);
            recv_command = (Command*)realloc(recv_command, recv_command->len);
            f2.read((char*)recv_command->message, recv_command->len-HEADER_LEN);
            recv_command->type = 2;
            std::cout <<recv_command->src_username << " -> " << recv_command->dst_username << ": message sended, bytes " << send(pfds[clients[recipiter].c_socket_pos].fd, recv_command, recv_command->len, 0) << std::endl;
            free(recv_command);
        }
        std::cout << "All messages sent" << std::endl;

    }
    
    f2.close();
    remove((recipiter+".bin").c_str());
}

void Event_AcceptNewClient(int listener){

    std::cout << "listener" << std::endl;
    int sock = accept(listener, NULL, NULL);

    num_clients++;
    pfds = (pollfd*)realloc(pfds, sizeof(pollfd)*(num_clients*2+1));

    pfds[num_clients*2-1].fd = sock;
    pfds[num_clients*2-1].events = POLLIN | POLLRDHUP | POLLHUP;
    pfds[num_clients*2-1].revents = 0;

    pfds[num_clients*2].fd = -1;
    pfds[num_clients*2].events = POLLIN;

    ClientInfo &new_client = clients["unind_user_"+std::to_string(num_unind_user)];

    new_client.c_socket_pos = num_clients*2-1;

    new_client.c_timer_pos = num_clients*2;

    new_client.head_recv_bytes = 0;

    new_client.message_recv_bytes = 0;

    new_client.current_command = (Command*)malloc(HEADER_LEN);

    sock_to_name[sock] = "unind_user_"+std::to_string(num_unind_user);

    num_unind_user++;
}

void DisconnectClient(std::string client_src_name, ClientInfo &client_data){
    std::cout << "User "<< client_src_name << " disconnected" << std:: endl;
        pollfd *socket_pfd = &pfds[client_data.c_socket_pos];
        pollfd *timer_pfd = &pfds[client_data.c_timer_pos];
        close(socket_pfd->fd);

        close(timer_pfd->fd);

        while(!client_data.commands_queue.empty()){
            save(client_src_name, client_data.commands_queue.front());
            client_data.commands_queue.pop();
        }

        free(client_data.current_command);

        clients[sock_to_name[pfds[num_clients*2-1].fd]].c_socket_pos = client_data.c_socket_pos;
        clients[sock_to_name[pfds[num_clients*2-1].fd]].c_timer_pos = client_data.c_timer_pos;
        int sock_to_delete = socket_pfd->fd;
        

        *socket_pfd = pfds[num_clients*2-1];
        *timer_pfd = pfds[num_clients*2];
        
        sock_to_name.erase(sock_to_delete);
        clients.erase(client_src_name);

        num_clients--;
        pfds = (pollfd*)realloc(pfds, sizeof(pollfd)*(num_clients*2+1));
}

void Event_DisconnectAll(){
    while(!clients.empty()){
        DisconnectClient(sock_to_name[pfds[1].fd], clients[sock_to_name[pfds[1].fd]]);
    }
}

bool AcceptCommand( ClientInfo &client_data){

    pollfd *socket_pfd = &pfds[client_data.c_socket_pos];
    int recv_bytes;

    if(client_data.head_recv_bytes != HEADER_LEN){
        recv_bytes = recv(socket_pfd->fd, (char*)client_data.current_command+client_data.head_recv_bytes, HEADER_LEN-client_data.head_recv_bytes, 0);
        if (recv_bytes == -1){
            perror("recv1");
            exit(1);
        }

        client_data.head_recv_bytes += recv_bytes;

        if(client_data.head_recv_bytes == HEADER_LEN){
            client_data.current_command = (Command*)realloc(client_data.current_command, client_data.current_command->len);
        }
    }
    else if(client_data.message_recv_bytes != (client_data.current_command->len)-HEADER_LEN){

        recv_bytes = recv(socket_pfd->fd, ((char*)client_data.current_command->message)+client_data.message_recv_bytes, (client_data.current_command->len-HEADER_LEN)-client_data.message_recv_bytes, 0);

        if (recv_bytes == -1){
            perror("recv2");
            exit(1);
        }

        client_data.message_recv_bytes += recv_bytes;
    
        if(client_data.message_recv_bytes == client_data.current_command->len-HEADER_LEN){

            char bufer[USERNAME_MAX_LEN+1];
            strncpy(bufer, client_data.current_command->src_username, USERNAME_MAX_LEN);
            bufer[USERNAME_MAX_LEN] = 0;
            std::string src_username = bufer;

            strncpy(bufer, client_data.current_command->dst_username, USERNAME_MAX_LEN);
            bufer[USERNAME_MAX_LEN] = 0;
            std::string dst_username = bufer;

            std::cout << "TYPE: " << client_data.current_command->type << std::endl;
            std::cout << "SRC: " << src_username << std::endl;
            std::cout << "DST: " << dst_username << std::endl;

            client_data.head_recv_bytes = 0;
            client_data.message_recv_bytes = 0;
            return true;
        }
    }
    return false;

}

void PrintInfoAboutPoll(const std::string client_src_name, const ClientInfo &client_data){
    pollfd pfds_tmp = pfds[client_data.c_socket_pos];
    printf("  fd=%s; events: %s%s%s%s\n", client_src_name.c_str(),
        (pfds_tmp.revents & POLLHUP) ? "POLLHUP " : "",
        (pfds_tmp.revents & POLLRDHUP)  ? "POLLRDHUP "  : "",
        (pfds_tmp.revents  & POLLIN)  ? "POLLIN "  : "",
        (pfds_tmp.revents  & POLLERR) ? "POLLERR " : "");
}

void ForwardMessage(std::string src_username, std::string dst_username,  Command *recv_command){

    std::cout << src_username << " -> " << dst_username << ": message sended, bytes " 
        << send(pfds[clients[dst_username].c_socket_pos].fd, recv_command, recv_command->len, 0) << std::endl;

    int tmp_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd_settime(tmp_timer, 0, &timer_settings, 0) == -1){
        perror("Timer");
        exit(1);
    }
    clients[dst_username].timers_queue.push(tmp_timer);
    if(pfds[clients[dst_username].c_timer_pos].fd == -1) {
        pfds[clients[dst_username].c_timer_pos].fd = tmp_timer;
    }
    
    Command *tmp = (Command*)malloc(recv_command->len);
    memcpy(tmp,recv_command,recv_command->len);
    clients[dst_username].commands_queue.push(tmp);
    free(tmp);
    tmp = nullptr;
    
}

void PopTimerAndComand(std::string username){
    close(clients[username].timers_queue.front());

    clients[username].timers_queue.pop();
    
    if(!clients[username].timers_queue.empty()){
        pfds[clients[username].c_timer_pos].fd = clients[username].timers_queue.front();
    }
    else{
        pfds[clients[username].c_timer_pos].fd = -1;
    }

    clients[username].commands_queue.pop();
}

void ForwardReply(std::string src_username, std::string dst_username, Command *answer){
    std::cout << src_username << " -> " << dst_username << ": message sended, bytes " 
        << send(pfds[clients[dst_username].c_socket_pos].fd, answer, answer->len, 0) << std::endl;         
    
    PopTimerAndComand(src_username);
}

void SaveMessage(const std::string src_username, std::string dst_username, Command *&recv_command){
    std::cout <<src_username << " -> " << dst_username << ": the recipient is offline"<< std::endl;

    save(dst_username, recv_command);

    std::swap(recv_command->dst_username, recv_command->src_username); 
    recv_command = (Command*)realloc(recv_command, HEADER_LEN+3);

    memcpy(recv_command->message, "300", 3);
    recv_command->len = HEADER_LEN+3;
    recv_command->type = 1;
    send(pfds[clients[src_username].c_socket_pos].fd, recv_command, recv_command->len, 0);

    std::swap(recv_command->dst_username, recv_command->src_username);
    
}

void IdentifyUser(const std::string client_src_name, ClientInfo &client_data, Command *recv_command){
    pollfd *socket_pfd = &pfds[client_data.c_socket_pos];
    char bufer[USERNAME_MAX_LEN+1];

    strncpy(bufer, client_data.current_command->src_username, USERNAME_MAX_LEN);
    bufer[USERNAME_MAX_LEN] = 0;
    std::string src_username = bufer;

    sock_to_name.erase(socket_pfd->fd);

    ClientInfo &current_client = clients[src_username];

    current_client.c_socket_pos = client_data.c_socket_pos;
    current_client.c_timer_pos = client_data.c_timer_pos;
    current_client.current_command = client_data.current_command;
    current_client.head_recv_bytes = 0;
    current_client.message_recv_bytes = 0;

    sock_to_name[socket_pfd->fd] = src_username;

    clients.erase(client_src_name);

    client_data = current_client;
    
    std::cout<<" LOAD " << src_username << std::endl;
    load_and_send(src_username);
}

void Event_ClientProcessing(){
    for(int sock_num = 1; sock_num < num_clients*2; sock_num = sock_num+2){
        std::string &client_src_name = sock_to_name[pfds[sock_num].fd] ;
        ClientInfo &client_data = clients[client_src_name];
        PrintInfoAboutPoll(client_src_name, client_data);
        pollfd *socket_pfd = &pfds[client_data.c_socket_pos];
        pollfd *timer_pfd = &pfds[client_data.c_timer_pos];

        if(socket_pfd->revents & POLLRDHUP || socket_pfd->revents & POLLHUP || timer_pfd->revents & POLLIN){
            DisconnectClient(client_src_name, client_data);
            sock_num = sock_num-2;
            continue;
        }   

        if(socket_pfd->revents & POLLIN){
            if(AcceptCommand(client_data)){
                char bufer[USERNAME_MAX_LEN+1];

                strncpy(bufer, client_data.current_command->src_username, USERNAME_MAX_LEN);
                bufer[USERNAME_MAX_LEN] = 0;
                std::string src_username = bufer;

                strncpy(bufer, client_data.current_command->dst_username, USERNAME_MAX_LEN);
                bufer[USERNAME_MAX_LEN] = 0;
                std::string dst_username = bufer;
                
                if(client_src_name.find("unind_user_") != std::string::npos){
                    IdentifyUser(client_src_name, client_data, client_data.current_command);
                }
                
                if(clients.count(dst_username)){
                    if(client_data.current_command->type == 0){
                        ForwardMessage(src_username, dst_username,client_data.current_command);
                    }
                
                    if(client_data.current_command->type == 1){
                        ForwardReply(src_username, dst_username, client_data.current_command);
                    }

                    continue;
                }

                if(!(clients.count(dst_username)) && client_data.current_command->type == 0){
                    SaveMessage(src_username, dst_username, client_data.current_command);   
                    continue;
                }
                
                //Если пришел ответ, а клиент, которому предназначался этот ответ отключился
                PopTimerAndComand(src_username);

            }
        }

    }
    
}


int main(int argc, char* argv[])
{
    if(argc != 2 ){
        Usage(argv[0]);
        exit(1);
    }

    int listener;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if(listener < 0)
    {
        perror("socket");
        exit(1);
    }

    char* p_end;
    long port = strtol(argv[1], &p_end, 10);

    if(*p_end!='\0' || port<0 || port>65535){
        Usage(argv[0]);
        perror("Wrong enter");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    pfds[0].fd = listener;
    pfds[0].events = POLLIN;
    timer_settings.it_value.tv_sec = 3;
    int ready;

    listen(listener, 5);
    
    std::cout << "Server is runnig" << std::endl;
    
    while(1)
    {
        //Маленькая справка по массиву pfds:
        //на 0 месте всегда стоит дескриптор слушающего сокета
        //далее на нечетных местах(1, 3, 5...) стоят дескрипторы сокетов подключенных к серверу клиентов
        //на четных местах(2, 4, 6...) стоят дескрипторы таймеров принятых клиентов
        //в итоге получается пара дескрипторов [сокет, таймер] для одного клиента в общем массиве дескрипторов. 
        ready = poll(pfds, num_clients*2+1, 3600000);
        
        printf("About to poll:\n");
        if (ready == -1 && errno != EINTR){
            perror("Poll");
            exit(1);
        }
        if(ready == 0){
            Event_DisconnectAll();
            break;
        }

        if(pfds[0].revents & POLLIN){

            clock_t start = clock();

            Event_AcceptNewClient(listener);

            clock_t end = clock();

            double seconds = (double)(end - start) / CLOCKS_PER_SEC;

            std::cout << "Client connection time: " << seconds << std::endl;

        }

        Event_ClientProcessing();
         
        std::cout << "end\n\n" << std::endl;
    }
    free(pfds);
    std::cout << "server is disabled"<<std::endl;
    return 0;
}