#include<iostream>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<poll.h>
#include<string.h>
#include<thread>
#include <mutex>
#include <vector>
#include <atomic>




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
};

static std::atomic<bool> finish_the_program(false);

// отправить сообщение
void send_message(int sock, unsigned int m_ID){

    Command *A = (Command*)malloc(24);
    
    char message_buff[1000];
    char s_buff[8];
    std::cout << "SRC: ";
    std::cin.ignore();
    std::cin.getline(s_buff, 8);
    
    // std::cout<< strlen(s_buff)<<std::endl;
    memcpy(A->src_username, s_buff, sizeof(A->src_username));

    std::cout << "DST: ";
    std::cin.getline(s_buff, 8);
    memcpy(A->dst_username, s_buff, sizeof(A->dst_username));

    std::cout << "Message: ";
    std::cin.getline(s_buff, 1000);
    A = (Command*)realloc(A, 24+(strlen(s_buff)+1));
    memcpy(A->message, s_buff, strlen(s_buff)+1);


    
    A->len = (strlen(s_buff)+1)+24;

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

    std::mutex _mutex;
    while(!finish_the_program){

        ready = poll(pfds, 1, 1);

        if (ready == -1){
            perror("poll");
            exit(2);
        }
        else if(pfds[0].revents & POLLIN){

            _mutex.lock();
            Command *data = (Command*)malloc(24); // для приема сообщения
                    
            bytes_read = recv(sock, data, 24, 0); // принимаем заголовок

            data = (Command*)realloc(data, data->len);// выделяем память под сообщение

            bytes_read = recv(sock, data->message, data->len-24, 0);

            r_mess.sender_username = data->dst_username;
            r_mess.message = data->message;
            r_mess.message_ID = data->message_ID;

            recv_messages->push_back(r_mess);
            _mutex.unlock();

            delete data;
            data = nullptr;
            pfds[0].revents = 0;
            
        }
    }
}

//просмотреть полученные сообщения
void read_messages(std::vector<RecvMessage> *recv_messages){
    for(RecvMessage i: *(recv_messages)){
        std::cout << "messageID: " << i.message_ID << "\n" << i.sender_username <<": " << i.message << std::endl << std::endl;
    }
}


void menu(int sock, std::vector<RecvMessage> *recv_messages){
    int act;
    int m_ID = 1;
    bool ex = 0 ;
    while(ex!=1){
        std::cout << "1 - send message" << std::endl;
        std::cout << "2 - read the recv message" << std::endl;
        std::cout << "0 - exit" << std::endl;

        std::cout << "choose an action: ";
        std::cin >>act;
        std::cout << std::endl;

        switch (act)
        {
        case(1):{
            /* code */
            send_message(sock, m_ID);
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
    
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("Problem with creating a socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(6666); 
    addr.sin_addr.s_addr = htonl((192<<24)+(168<<16)+(10<<8)+126);
    
    if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Problem with connection");
        exit(2);
    }

    std::vector<RecvMessage> recv_messages;
    std::thread t1{recv_messages_in_thread, sock, &recv_messages};
    menu(sock, &recv_messages);
    
    t1.join();
    return 0;
}