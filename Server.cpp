#include<iostream>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <unistd.h>



struct Command{
    unsigned short len;
    unsigned short type;
    unsigned int message_ID;
    char src_username[8];
    char dst_username[8];
    char message[];
};

int main()
{
    int sock, listener;
    struct sockaddr_in addr;
    int bytes_read;
    unsigned short len_buff;
    char buff_message[1000];
    


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

    listen(listener, 1);
    
    while(1)
    {
        
        sock = accept(listener, NULL, NULL);
        if(sock < 0)
        {
            perror("accept");
            exit(3);
        }
        else{
            std::cout << "sock is opened" << std::endl;
        }
     
        Command *data = (Command*)malloc(24);
    
        bytes_read = recv(sock, data, 24, 0);

        data = (Command*)realloc(data, data->len);

        bytes_read = recv(sock, data->message, data->len-24, 0);

        
        std::cout << bytes_read << std::endl;
        bytes_read = recv(sock, &data, sizeof(data), 0);
        std::cout << data->len << std::endl;
        std::cout << data->type << std::endl;
        std::cout << data->message_ID << std::endl;
        std::cout << data->src_username << std::endl;
        std::cout << data->dst_username << std::endl;
        std::cout << data->message << std::endl;
        

        close(sock);

        delete data;
        data = nullptr;
        
        std::cout << "sock is closed" << std::endl;

    }
    std::cout << "ogg" << std::endl;
    return 0;
}