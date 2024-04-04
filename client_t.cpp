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

#define SWAP(type, a, b) type tmp = a; a = b; b = tmp;


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

static std::atomic<bool> finish_the_program(false);

// отправить сообщение
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

// получать сообщения
void recv_messages_in_thread(int sock, std::vector<RecvMessage> *recv_messages){
    struct sockaddr_in addr;
    struct pollfd *pfds = (pollfd*)malloc(sizeof(struct pollfd)*1);
    struct RecvMessage r_mess;

    pfds[0].fd = sock;
    pfds[0].events = POLLIN ;
    int ready;
    int bytes_read;

    int tmp = 0;//УБРАТЬ

    std::mutex _mutex;
    while(!finish_the_program){

        ready = poll(pfds, 1, 1);

        if (ready == -1){
            perror("poll");
            exit(3);
        }
        else if(pfds[0].revents & POLLIN){

            _mutex.lock();
            Command *data = (Command*)malloc(24); // для приема сообщения
                    
            bytes_read = recv(sock, data, 24, 0); // принимаем заголовок

            data = (Command*)realloc(data, data->len);// выделяем память под сообщение

            bytes_read = recv(sock, data->message, data->len-24, 0);

            r_mess.sender_username = data->src_username;
            r_mess.message = data->message;
            r_mess.message_ID = data->message_ID;
            r_mess.message_type = data->type;

            recv_messages->push_back(r_mess);
            _mutex.unlock();

            if(data->type == 0 ){
                for(int i = 0; i < 8; i++){
                    SWAP(char, data->dst_username[i], data->src_username[i]); //для отправки ответа клиенту отправителю от лица неподключенного клиента получателя 
                }

                data = (Command*)realloc(data, 28);//на заголовок и сообщение/ответ "200"

                memcpy(data->message, "200", 4);
                data->len = 28;
                data->type = 1;


                send(sock, data, data->len, 0);

            }

            delete data;
            data = nullptr;
            pfds[0].revents = 0;
            
        }
    }
}

//просмотреть полученные сообщения
void read_messages(std::vector<RecvMessage> *recv_messages){
    for(RecvMessage i: *(recv_messages)){
        std::cout<<"Message type: "<<i.message_type << "\n" << "Message ID: " << i.message_ID << "\n" << i.sender_username <<": " << i.message << std::endl << std::endl;
    }
}


void menu(int sock, std::vector<RecvMessage> *recv_messages, char *src_username){
    int act;
    int m_ID = 1;
    bool ex = 0 ;
    while(ex!=1){
        std::cout << "1 - send message" << std::endl;
        std::cout << "2 - read the recv message" << std::endl;
        std::cout << "0 - exit" << std::endl;

        std::cout << "choose an action: ";
        std::cin >>act;
        std::cin.clear();
        std::cout << std::endl;

        switch (act)
        {
        case(1):{
            /* code */
            send_message(sock,src_username, m_ID);
            m_ID++;
            break;
        }
        case(2):{
            /* прочитать сообщения из переменной */
            read_messages(recv_messages);
            break;
        }
        case(0):{
            close(sock);
            ex = 1;
            finish_the_program = true;
            break;
        }

        default:
            std::cout << "wrong enter" <<std::endl; 
            break;
        }

    }
    

}

int main(int argc, char* argv[]){
    
    std::cout  << argc << std::endl;
    
    if((argc != 4) || (strlen(argv[1])>7) ){
        std::cout << "wrong arguments" << std::endl;
        perror("enter");
        exit(4);
    }

    //данные для подключения
    char src_username[8];
    char* p_end;
    memcpy(src_username, argv[1],8);
    int ip = inet_addr( argv[2] );
    int port = htons(strtol(argv[3], &p_end, 10));
    
    if(strlen(p_end)!=0){
        perror("enter");
        exit(4);
    }
    
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("Problem with creating a socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    // addr.sin_port = htons(6666); 
    // addr.sin_addr.s_addr = htonl((192<<24)+(168<<16)+(10<<8)+126);
    addr.sin_port = port; 
    addr.sin_addr.s_addr = ip;
    
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