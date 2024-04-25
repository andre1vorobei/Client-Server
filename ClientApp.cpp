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

struct Command{
    unsigned short len;
    unsigned short type;
    unsigned int message_ID;
    char src_username[8];
    char dst_username[8];
    char message[];
};

void Usage(char *program_name){
    std::cout << "Usage: " << program_name << " <Username> <IP> <Port>" << std::endl;
}

void SendAnswer(int sock, Command *&data){
    std::swap(data->dst_username, data->src_username); //для отправки ответа клиенту отправителю от лица клиента получателя 
    
    data = (Command*)realloc(data, 28);//на заголовок и сообщение/ответ "200"

    memcpy(data->message, "200", 4);
    data->len = 28;
    data->type = 1;

    send(sock, data, data->len, 0);

}

void RecvMessage(int sock){

    Command *data = (Command*)malloc(24);
    recv(sock, data, 24, 0);
    data = (Command*)realloc(data, data->len);
    recv(sock, data->message, data->len-24, 0);

    if(data->type == 0){
        std::cout << std::endl;
        std::cout << "New message by " << data->src_username << ": " << data->message << std::endl;
        SendAnswer(sock, data);
    }
    else if(data->type == 2){
        std::cout << std::endl;
        std::cout << "New Offline message by " << data->src_username << ": " << data->message << std::endl;
    }

    free(data);
}


void SendMessage(int sock, std::string &str_buff, char *_src_username){
    static unsigned int _message_ID = 1;
    std::string _dst_username = str_buff.substr(0, str_buff.find(':'));
    std::string _message = str_buff.substr(str_buff.find(':')+1);

    if(_dst_username.length()>7 || _message.length()>999 || _dst_username.length()==0 || _message.length()==0 || str_buff.find(':') == -1){
        std::cout << "Wrong enter, try again" << std::endl;
    }
    else{
        Command *sended_message = (Command*)malloc(24+_message.length()+1);
        memcpy(sended_message->src_username, _src_username, 8);
        memcpy(sended_message->dst_username, _dst_username.c_str(), _dst_username.length()+1);
        memcpy(sended_message->message, _message.c_str(), _message.length()+1);
        sended_message->len = 24+_message.length()+1;
        sended_message->message_ID = _message_ID;
        _message_ID++;
        sended_message->type = 0;

        send(sock, sended_message, sended_message->len, 0);
        free(sended_message);
    }
}

int main(int argc, char* argv[]){
    if(argc != 4){
        Usage(argv[0]);
        exit(1);
    }
    if((strlen(argv[1])>7) ){
        Usage(argv[0]);
        perror("Too long username, max length is 7");
        exit(1);
    }

    struct in_addr inp;
    int sock;
    struct sockaddr_in addr;
    char src_username[8];
    char* p_end;

    memcpy(src_username, argv[1],8);
    long port = strtol(argv[3], &p_end, 10);
    
    if(*p_end!='\0' || port<0 || port>65535 || !inet_aton( argv[2] , &inp)/*ip*/){
        Usage(argv[0]);
        perror("Wrong format of IP or Port");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
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
    bool ex = false;

    while(!ex){

        ready = poll(pfds, 2, 1000);

        if (ready == -1){
            perror("poll");
            exit(1);
        }
        else if((pfds[1].revents & POLLRDHUP) || (pfds[1].revents & POLLHUP)){
            std::cout << "server is disable"<<std::endl;
            ex = true;
        }
        else if(pfds[0].revents & POLLIN){
            std::cout << (pfds[0].revents & POLLIN) << std::endl;
            std::getline(std::cin, str_buff);
            if(str_buff == "exit"){
                ex = true;
            }
            else{
                SendMessage(sock, str_buff, src_username);
            }
        }
        else if(pfds[1].revents & POLLIN){
            RecvMessage(sock);
        }
    }
    std::cout << "programm finishd" << std::endl;
    return 0;
}