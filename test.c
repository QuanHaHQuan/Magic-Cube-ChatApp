#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mysql/mysql.h>

#define SZMESSAGEBUFFER 4096
#define N_FDS_INIT 4096

int handleData(const char* buf,int bufLen,int confd)
{
    static char sqlStr[1024];
    static char msg[4096];
    static char rmsg[4096];
    static char uName[64];
    static char ruName[64];
	static char pWord[64];
	static char type[3];
	MYSQL mysql;
    MYSQL_RES *result=NULL;
    MYSQL_ROW row;
    mysql_init(&mysql);
    if(mysql_real_connect(&mysql,"127.0.0.1","root","Chenhaoquan123456","qq",0,NULL,0)==NULL)
	{
		printf("connect error:%s\n",mysql_error(&mysql));
        goto errlbl;
	}
	printf("%s\n",buf);
	strncpy(type,buf+1,2);
	printf("%s\n",type);
	if(strcmp(type,"01")==0)//sign up
	{

		sscanf(buf+4,"%[^|]|%s",uName,pWord);//#01|andy|123
		//printf("uName=%s\npWord=%s\n",uName,pWord);
		sprintf(sqlStr,"SELECT * FROM UserInfo WHERE UserName='%s'",uName);
		if(mysql_query(&mysql,sqlStr)!=0)
		{
			printf("query error:%s\n",mysql_error(&mysql));
			goto errlbl;
		}
		result=mysql_store_result(&mysql);
		if(result==NULL)
		{
			printf("result error:%s\n",mysql_error(&mysql));
			goto errlbl;
		}
		row=mysql_fetch_row(result);
		if(row) // username already existed 
		{
			write(confd,"#01|1",5);
		}
		else
		{
			write(confd,"#01|0",5);
			sprintf(sqlStr,"INSERT INTO UserInfo VALUES('%s','%s',-1)",uName,pWord);
			if(mysql_query(&mysql,sqlStr)!=0)
	        {
        	    printf("query error:%s\n",mysql_error(&mysql));
                goto errlbl;
            }
		}
	}
	else if(strcmp(type,"02")==0)//sign in
	{
        sscanf(buf+4,"%[^|]|%s",uName,pWord);//01|andy|123
        //printf("uName=%s\npWord=%s\n",uName,pWord);
        sprintf(sqlStr,"SELECT * FROM UserInfo WHERE UserName='%s'",uName);
        if(mysql_query(&mysql,sqlStr)!=0)
        {
            printf("query error:%s\n",mysql_error(&mysql));
            goto errlbl;
        }
        result=mysql_store_result(&mysql);
        if(result==NULL)
        {
            printf("result error:%s\n",mysql_error(&mysql));
            goto errlbl;
        }
        row=mysql_fetch_row(result);
		if(!row)
		{
			write(confd,"#02|1",5);//user does not exist
		}
		else
		{
            sprintf(sqlStr,"SELECT * FROM UserInfo WHERE UserName='%s' AND UserPassword = '%s'",uName,pWord);
            if(mysql_query(&mysql,sqlStr)!=0)
            {
                printf("query error:%s\n",mysql_error(&mysql));
                goto errlbl;
            }
            result=mysql_store_result(&mysql);
            if(result==NULL)
            {
                printf("result error:%s\n",mysql_error(&mysql));
                goto errlbl;
            }
            row=mysql_fetch_row(result);
            if(!row)
            {
                write(confd,"#02|2",5);
            }
            else
            {
                sprintf("SELECT ConnectSocketFD FROM UserInfo WHERE UserName='%s'",uName);
                if(mysql_query(&mysql,sqlStr)!=0)
                {
                    printf("query error:%s\n",mysql_error(&mysql));
                    goto errlbl;
                }
                result=mysql_store_result(&mysql);
                if(result==NULL)
                {
                    printf("result error:%s\n",mysql_error(&mysql));
                    goto errlbl;
                }
                row=mysql_fetch_row(result);
                if(atoi(row[0])!=-1)
                {
                    write(atoi(row[0]),"#02|3",5);//
                }
                else
                {
                    sprintf("UPDATE UserInfo SET ConnectSocketFD = %d WHERE UserName='%s'",confd,uName);
                    if(mysql_query(&mysql,sqlStr)!=0)
                    {
                        printf("query error:%s\n",mysql_error(&mysql));
                        goto errlbl;
                    }
                    write(confd,"#02|0",5);
                }
            }
		}
	}
    /*else if(strcmp(type,"03")==0)
    {
        
    }
    else if(strcmp(type,"04")==0)// sending message
    {
        sscanf(buf+4,"%[^|]|%4096s",ruName,msg);//#03|sent to whom|msg
        sprintf(sqlStr,"SELECT ConnectSocketFD FROM UserInfo WHERE UserName='%s'",ruName);
        if(mysql_query(&mysql,sqlStr)!=0)
        {
            printf("query error:%s\n",mysql_error(&mysql));
            goto errlbl;
        }
        result=mysql_store_result(&mysql);
        if(result==NULL)
        {
            printf("result error:%s\n",mysql_error(&mysql));
            goto errlbl;
        }
        row=mysql_fetch_row(result);
        if(atoi(row[0])!=-1)
        {
            sprintf(sqlStr,"SELECT UserName FROM UserInfo WHERE ConnectSocketFD='%d'",confd);
            if(mysql_query(&mysql,sqlStr)!=0)
            {
                printf("query error:%s\n",mysql_error(&mysql));
                goto errlbl;
            }
            result=mysql_store_result(&mysql);
            if(result==NULL)
            {
                printf("result error:%s\n",mysql_error(&mysql));
                goto errlbl;
            }
            row=mysql_fetch_row(result);
            sprintf(rmsg,"#03|%s|%s",row[0],msg);
            write(atoi(row[0]),rmsg,strlen(rmsg));
        }  
    }
    else if ()//new head img
*/
	mysql_free_result(result);
	mysql_close(&mysql);
	return 0;
    errlbl:
    mysql_close(&mysql);
    return -1;
}

int main(){
    int serverSocketFD=socket(AF_INET,SOCK_STREAM/*|SOCK_CLOEXEC*/,IPPROTO_TCP);/*IPPROTO_UDP*/

    if(serverSocketFD==-1){
        printf("error in socket()\n");
        return 0;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr,0,sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);//inet_addr("127.0.0.1");
    serverAddr.sin_port=htons(1234);

    if(bind(serverSocketFD,(struct sockaddr*)&serverAddr,sizeof(serverAddr))==-1){
        printf("error in bind()\n");
        return 0;
    }

    if(listen(serverSocketFD,SOMAXCONN)==-1){
        printf("error in listen()\n");
        return 0;
    }

    struct pollfd *allFDs=(struct pollfd *)malloc(N_FDS_INIT*sizeof(struct pollfd));
    nfds_t allFDsLen=1,allFDsCap=N_FDS_INIT,nConnections=0,i;
    allFDs[0].fd=serverSocketFD;
    allFDs[0].events=POLLIN;

    if(!allFDs){
        printf("error in malloc()\n");
        return 0;
    }

    struct sockaddr_in *connectAddrs=(struct sockaddr_in *)malloc(N_FDS_INIT*sizeof(struct sockaddr_in));
    socklen_t connectAddrLen;
    if(!connectAddrs){
        printf("error in malloc()\n");
        free(allFDs);
        return 0;
    }
    int connectSocketFD;
    static char messageBuffer[SZMESSAGEBUFFER];
    char c='y';
    while((c|0x20)=='y'){
        int nReady=poll(allFDs,allFDsLen,-1);
        if(nReady==-1){
            printf("error in poll()\n");
            continue;
        }
        if(allFDs[0].revents&POLLIN){
            for(i=1;allFDs[i].fd!=-1&&i<allFDsLen;++i);
            connectAddrLen=sizeof(struct sockaddr_in);
            connectSocketFD=accept(serverSocketFD,(struct sockaddr*)(connectAddrs+i),&connectAddrLen);
            if(connectSocketFD==-1){
                printf("error in accept()\n");
                continue;
            }
            allFDs[i].fd=connectSocketFD;
            allFDs[i].events=POLLIN;
            if(i==allFDsLen){
                ++allFDsLen;
            }
            if(allFDsLen==allFDsCap){
                allFDsCap<<=1;
                allFDs=(struct pollfd *)realloc(allFDs,allFDsCap*sizeof(struct pollfd));
                connectAddrs=(struct sockaddr_in *)realloc(connectAddrs,allFDsCap*sizeof(struct sockaddr_in));
                if(!(allFDs&&connectAddrs)){
                    printf("error in realloc()\n");
                    free(allFDs);
                    free(connectAddrs);
                    return 0;
                }
            }
            printf("Connected to client.\n");
            printf("Client address:%s\nClient port:%d\n",inet_ntoa(connectAddrs[i].sin_addr),ntohs(connectAddrs[i].sin_port));
            ++nConnections;
            continue;
        }
        for(i=1;i<allFDsLen;++i){
            if(allFDs[i].revents&POLLIN){
                connectSocketFD=allFDs[i].fd;
                int nRead=read(connectSocketFD,messageBuffer,SZMESSAGEBUFFER-1);
                if(nRead){
                    if(nRead==-1){
                        printf("error in read()\n");
                        continue;
                    }
                    messageBuffer[nRead]=0;
                    handleData(messageBuffer,nRead,connectSocketFD);
                    continue;
                }
                printf("Connection closed.\n");
                printf("Client address:%s\nClient port:%d\n",inet_ntoa(connectAddrs[i].sin_addr),ntohs(connectAddrs[i].sin_port));
                allFDs[i].fd=-1;
                while(allFDsLen&&allFDs[allFDsLen-1].fd==-1){
                    --allFDsLen;
                }
                --nConnections;
            }
        }
        if(!nConnections){
            printf("No connection, continue? (y/n)\n");
            while((c=getchar())==0x0a||c==0x0d||c==0x20);
        }
    }
    free(allFDs);
    free(connectAddrs);
    return 0;
}