#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SZMESSAGEBUFFER 4096

int main(){
    int clientSocketFD=socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK/*|SOCK_CLOEXEC*/,IPPROTO_TCP);/*IPPROTO_UDP*/

    if(clientSocketFD==-1){
        printf("error in socket()\n");
        return 0;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr,0,sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);//inet_addr("127.0.0.1");
    serverAddr.sin_port=htons(1234);

    while(connect(clientSocketFD,(struct sockaddr*)&serverAddr,sizeof(serverAddr))==-1);
    printf("Connected to server.\n");

    int f=1;
    printf("Command:\n");
    char c;
    while((c=getchar())==0x0a||c==0x0d||c==0x20);
    static char buf[SZMESSAGEBUFFER];
    while(f){
        switch(c){
            case 'r':{
                int nrd=read(clientSocketFD,buf,SZMESSAGEBUFFER-1);
                if(nrd==-1){
                    printf("error in read()\n");
                    break;
                }
                buf[nrd]=0;
                printf("Message received:\n%s\n",buf);
                break;
                }
            case 'w':{
                printf("Input message to send:\n");
                scanf("%63s",buf);
                write(clientSocketFD,buf,strlen(buf));
                break;
                }
            default:{
                f=0;
                printf("Invalid command, input any command to quit.\n");
                break;
                }
        }
        printf("Command:\n");
        while((c=getchar())==0x0a||c==0x0d||c==0x20);
    }
    close(clientSocketFD);
    printf("Connection closed.\n");
    return 0;
}