#include<iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include<string.h>

struct Command{
    unsigned short len;
    unsigned short type;
    unsigned int message_ID;
    char src_username[8];
    char dst_username[8];
    char message[];
};

void send(int socket, const char* data, unsigned short)
{
	send(socket, data, sizeof(data), 0);
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


    Command *A = (Command*)malloc(24);
    

    char message_buff[1000];
    char s_buff[8];
    std::cout << "SRC: ";
    std::cin.getline(s_buff, 8);
    
    std::cout<< strlen(s_buff)<<std::endl;
    memcpy(A->src_username, s_buff, sizeof(A->src_username));

    std::cout << "DST: ";
    std::cin.getline(s_buff, 8);
    memcpy(A->dst_username, s_buff, sizeof(A->dst_username));

    std::cout << "Message: ";
    std::cin.getline(s_buff, 1000);
    A = (Command*)realloc(A, 24+(strlen(s_buff)+1));
    memcpy(A->message, s_buff, strlen(s_buff)+1);


    
    A->len = (strlen(s_buff)+1)+24;
    std::cout << A->len <<std::endl;
    std::cout << A->message <<std::endl;

    A->type = 1;
    A->message_ID = 1;
    std::cout << sizeof(A) << std::endl;
    std::cout << A->type << std::endl;
    std::cout << A->message_ID << std::endl;
    std::cout << A->src_username << std::endl;
    std::cout << A->dst_username << std::endl;
    std::cout << A->message << std::endl;
    send(sock, A, A->len, 0);
    
    close(sock);

    return 0;
}