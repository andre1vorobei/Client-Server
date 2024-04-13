//Для запуска клиента: в комарндной строке указать сначала свой username длинной < 8 символов, затем айпиадрес сервера, затем порт сервера

#include<iostream>
#include<string>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <arpa/inet.h>
#include<unistd.h>
#include<poll.h>
#include<string.h>
#include<thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <limits>



struct Command{
    unsigned short len;
    unsigned short type;
    unsigned int message_ID;
    char src_username[8];
    char dst_username[8];
    char message[];
};

struct RecvMessage{
    std::string sender_username;
    std::string message;
    unsigned int message_ID;
    unsigned short message_type;
};

std::atomic<bool> finish_the_program(false);
std::atomic<bool> flag_to_read(true);

void send_message(int sock,char *SRC, unsigned int m_ID){

    Command *A = (Command*)malloc(24);
    
    char s_buff[1000];
    std::string str_buff;
    
    memcpy(A->src_username, SRC, 8);

    
    std::cout << "DST: ";
    std::cin.ignore();
    getline(std::cin, str_buff);

    while(str_buff.length()>7 || str_buff.length()==0){
        std::cout <<( str_buff.length()==0 ? "Username cant be empty" : "Too long username, try again") << std::endl;
        std::cout << "DST: ";
        getline(std::cin, str_buff);
    }
    memcpy(A->dst_username, str_buff.c_str(), 8);

 
    std::cout << "Message: ";
    getline(std::cin, str_buff);
     while(str_buff.length()>999 || str_buff.length()==0){
        std::cout << (str_buff.length()==0 ? "Message cant be empty" : "Too long message, try again") << std::endl;
        std::cout << "Message: ";
        getline(std::cin, str_buff);
    }

    A = (Command*)realloc(A, 24+str_buff.length()+1);
    
    memcpy(A->message, str_buff.c_str(), str_buff.length()+1);
    std::cout <<  str_buff.length()+1 << " " << str_buff << std::endl;

    A->len = ((str_buff.length()+1)+24);

    A->type = 0;
    A->message_ID = m_ID;

    send(sock, A, A->len, 0);
    std::cout << std::endl;

}

void recv_messages_in_thread(int sock, std::vector<RecvMessage> *recv_messages){
    struct sockaddr_in addr;
    struct pollfd *pfds = (pollfd*)malloc(sizeof(struct pollfd));
    struct RecvMessage r_mess;

    pfds[0].fd = sock;
    pfds[0].events = POLLIN ;
    int ready;
    int bytes_read;

    while(!finish_the_program.load()){

        ready = poll(pfds, 1, 1);

        if (ready == -1){
            perror("poll");
            exit(3);
        }
        else if(pfds[0].revents & POLLIN){

            Command *data = (Command*)malloc(24);
                    
            bytes_read = recv(sock, data, 24, 0);

            data = (Command*)realloc(data, data->len);

            bytes_read = recv(sock, data->message, data->len-24, 0);

            r_mess.sender_username = data->src_username;
            r_mess.message = data->message;
            r_mess.message_ID = data->message_ID;
            r_mess.message_type = data->type;

            if(data->type == 0 ){
                for(int i = 0; i < 8; i++){
                    std::swap(data->dst_username[i], data->src_username[i]); //для отправки ответа клиенту отправителю от лица неподключенного клиента получателя 
                }

                data = (Command*)realloc(data, 28);//на заголовок и сообщение/ответ "200"

                memcpy(data->message, "200", 4);
                data->len = 28;
                data->type = 1;


                send(sock, data, data->len, 0);

            }

            while(!flag_to_read.load()){}
            flag_to_read.store(false);
            recv_messages->push_back(r_mess);
            flag_to_read.store(true);

            delete data;
            data = nullptr;
            pfds[0].revents = 0;
            
        }
    }
}

void read_messages(std::vector<RecvMessage> *recv_messages){
    while(!flag_to_read.load()){}

    flag_to_read.store(false);
    for(RecvMessage i: *(recv_messages)){
        std::cout<<"Message type: "<<i.message_type << "\n" << "Message ID: " << i.message_ID << "\n" << i.sender_username <<": " << i.message << std::endl << std::endl;
    }
    flag_to_read.store(true);

    
}


void menu(int sock, std::vector<RecvMessage> *recv_messages, char *src_username){
    std::string act_buff;
    int act;
    int m_ID = 1;
    bool ex = 0 ;
    char *end_p;
    while(ex!=1){
        std::cout << "1 - send message" << std::endl;
        std::cout << "2 - read the recv message" << std::endl;
        std::cout << "0 - exit" << std::endl;

        std::cout << "choose an action: ";
        std::cin >>act_buff;
        act = strtol(act_buff.c_str(), &end_p, 10);
        while(strlen(end_p)!=0){
            std::cout << "wrong enter, try again"<<std::endl;
            std::cout << "choose an action: ";
            std::cin >>act_buff;
            act = strtol(act_buff.c_str(), &end_p, 10);
        }

        std::cout << std::endl;

        switch (act)
        {
        case(1):{
            send_message(sock,src_username, m_ID);
            m_ID++;
            break;
        }
        case(2):{
            read_messages(recv_messages);
            break;
        }
        case(0):{
            close(sock);
            ex = 1;
            finish_the_program.store(true);
            break;
        }

        default:
            std::cout << "wrong enter\n" <<std::endl; 
            break;
        }

    }
    

}

int main(int argc, char* argv[]){
    
    if((argc != 4) || (strlen(argv[1])>7) ){
        perror("Wrong enter");
        exit(4);
    }

    struct in_addr inp;
    int sock;
    struct sockaddr_in addr;
    char src_username[8];
    char* p_end;

    memcpy(src_username, argv[1],8);
    long port = strtol(argv[3], &p_end, 10);
    
    if(strlen(p_end)!=0 || port<0 || port>65535 || !inet_aton( argv[2] , &inp)/*ip*/){
        perror("Wrong enter");
        exit(4);
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
        exit(2);
    }

    std::vector<RecvMessage> recv_messages;

    std::thread t1{recv_messages_in_thread, sock, &recv_messages};

    menu(sock, &recv_messages, src_username);
    
    t1.join();
    return 0;
}