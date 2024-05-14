//Для запуска клиента: в комарндной строке указать сначала свой username длинной < 8 символов, затем айпиадрес сервера, затем порт сервера

#include<iostream>
#include<string>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<poll.h>
#include<string.h>

#include <unistd.h>

#define USERNAME_MAX_LEN 8
#define HEADER_LEN sizeof(Header)
#define MESSAGE_MAX_LEN 1000

struct Header{
    unsigned short len;
    unsigned short type;
    unsigned int message_ID;
    char src_username[USERNAME_MAX_LEN];
    char dst_username[USERNAME_MAX_LEN];
};

struct Command{
    Header header;
    char message[];
};


void Usage(char *program_name){
    std::cout << "Usage: " << program_name << " <Username> <IP> <Port>" << std::endl;
}

void SendAnswer(int sock, Command *&data){
    std::swap(data->header.dst_username, data->header.src_username); //для отправки ответа клиенту отправителю от лица клиента получателя 
    
    data = (Command*)realloc(data, HEADER_LEN+3);//на заголовок и сообщение/ответ "200"

    memcpy(data->message, "200", 3);
    data->header.len = HEADER_LEN+3;
    data->header.type = 1;

    send(sock, data, data->header.len, 0);

}

void RecvMessage(int sock){
    
    static int head_recv_bytes = 0;
    static int message_recv_bytes = 0;
    static Command *data = (Command*)malloc(HEADER_LEN);
    int recv_bytes;

    if(head_recv_bytes != HEADER_LEN){

        recv_bytes = recv(sock, (char*)data+head_recv_bytes, HEADER_LEN-head_recv_bytes, 0);
        if (recv_bytes == -1){
            perror("recv");
            exit(1);
        }

        head_recv_bytes += recv_bytes;
        if(head_recv_bytes == HEADER_LEN){
            data = (Command*)realloc(data, data->header.len);
        }
    }
    else if(message_recv_bytes != data->header.len-HEADER_LEN){

        recv_bytes =  recv(sock, ((char*)data->message)+message_recv_bytes, (data->header.len-HEADER_LEN)-message_recv_bytes, 0);
        if (recv_bytes == -1){
            perror("recv");
            exit(1);
        }

        message_recv_bytes += recv_bytes;
    
        if(message_recv_bytes == data->header.len-HEADER_LEN){
            char *mes = new char[data->header.len-HEADER_LEN+1];
            memcpy(mes, data->message, data->header.len-HEADER_LEN+1);
            mes[data->header.len-HEADER_LEN] = '\0';

            // char *us_name = new char[strlen(data->header.src_username)+1];
            // memcpy(us_name, data->header.src_username, strlen(data->header.src_username)+1);
            // us_name[data->header.len-HEADER_LEN] = '\0';

            char us_name[USERNAME_MAX_LEN];
            memcpy(us_name, data->header.src_username, USERNAME_MAX_LEN);

            if(data->header.type == 0){
                SendAnswer(sock, data);

                std::cout << std::endl;
                std::cout << "From " << us_name << ": " << mes << "\n" << std::endl;
            }
            else if(data->header.type == 1){
                std::cout << std::endl;
                std::cout<< "User: " << us_name << "  Message_ID: " << data->header.message_ID << "  Status: " << mes << "\n" << std::endl;
            }
            else if(data->header.type == 2){
                std::cout << std::endl;
                std::cout << "Offline from " << us_name << ": " << mes << "\n" << std::endl;
            }

            free(data);
            delete mes;
            mes = nullptr;

            data = (Command*)malloc(HEADER_LEN);
            head_recv_bytes = 0;
            message_recv_bytes = 0;

        }
    }
}


void SendMessage(int sock, std::string &str_buff, char *own_src_username){
    static unsigned int _message_ID = 0;
    char dst_username[USERNAME_MAX_LEN];
    std::string tmp_dst_username = str_buff.substr(0, str_buff.find(':'));
    std::string message = str_buff.substr(str_buff.find(':')+1);

    if(tmp_dst_username.length() > USERNAME_MAX_LEN || message.length() > MESSAGE_MAX_LEN || tmp_dst_username.length() == 0 || message.length() == 0 || str_buff.find(':') == std::string::npos){
        std::cout << "Wrong enter, try again" << std::endl;
        return;
    }

    memcpy(dst_username, tmp_dst_username.c_str(), USERNAME_MAX_LEN);
    
    Command *prepared_command = (Command*)malloc(HEADER_LEN+message.length());

    memcpy(prepared_command->header.src_username, own_src_username, USERNAME_MAX_LEN);// own_src_username создается в мейне как char[8], так что он должен полностью копироваться в prepared_command
    memcpy(prepared_command->header.dst_username, dst_username, USERNAME_MAX_LEN);
    memcpy(prepared_command->message, message.c_str(), message.length());

    prepared_command->header.len = HEADER_LEN+message.length();
    prepared_command->header.message_ID = _message_ID;
    _message_ID++;
    prepared_command->header.type = 0;

    send(sock, prepared_command, prepared_command->header.len, 0);
    free(prepared_command);
    
}

int main(int argc, char* argv[]){
    if(argc != 4){
        Usage(argv[0]);
        exit(1);
    }
    if((strlen(argv[1])>USERNAME_MAX_LEN) ){
        Usage(argv[0]);
        perror("Too long username, max length is 8");
        exit(1);
    }

    struct in_addr inp;
    int sock;
    struct sockaddr_in addr;
    char src_username[USERNAME_MAX_LEN];
    char* p_end;

    memcpy(src_username, argv[1],USERNAME_MAX_LEN);
    long port = strtol(argv[3], &p_end, 10);
    if(*p_end != '\0' || port < 0 || port > 65535 || !inet_aton(argv[2], &inp)/*ip*/){
        Usage(argv[0]);
        perror("Wrong format of IP or Port");
        exit(1);
    }

    addr.sin_family = AF_INET;
    inet_aton("192.168.10.126", &inp);
    addr.sin_port = htons(port );
    addr.sin_addr.s_addr = inp.s_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if(sock < 0)
    {
        perror("Problem with creating a socket");
        exit(1);
    }
    
    if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Problem with connection");
        exit(1);
    }
    else{
        std::cout << "Connection success\n" << std::endl;
        std::cout << "Available commands:\n" << std::endl;
        std::cout << "<dst_username>:<message> - send the message to another user\n" << std::endl;
        std::cout << "exit - lol to exit\n\n" << std::endl;
    }

    pollfd *pfds = (pollfd*)malloc(sizeof(pollfd)*2);
    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[1].fd = sock;
    pfds[1].events = POLLIN | POLLRDHUP | POLLHUP;

    int ready;

    int message_number = 0;

    std::string str_buff;

    while(true){

        ready = poll(pfds, 2, 1);

        if (ready == -1 && errno != EINTR){
            perror("poll");
            exit(1);
        }

        if((pfds[1].revents & POLLRDHUP) || (pfds[1].revents & POLLHUP)){
            std::cout << "server is down"<<std::endl;
            break;
        }

        if(pfds[0].revents & POLLIN){
            std::getline(std::cin, str_buff);
            if(str_buff == "exit"){
                break;
            }
            else{
                SendMessage(sock, str_buff, src_username);
            }
        }

        if(pfds[1].revents & POLLIN){
            RecvMessage(sock);
        }
    }
    std::cout << "programm finishd" << std::endl;
    return 0;
}