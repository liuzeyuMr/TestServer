#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <signal.h>
#include <wait.h>
#include "warp.h"
#define MAXLINE 4096
#define SERV_PORT 6666
/**/
//Socket简单大小写转换
int Server_toupper(){
    printf("Hello, World! this is my first code! \n");
    int server_sockfd , connfd;
    struct  sockaddr_in seraddr ,client_addr;
    socklen_t  clie_addr_len;
    char buff[MAXLINE];
    char clie_buff[MAXLINE];
    int n;
    server_sockfd = Socket(AF_INET,SOCK_STREAM,0);

    memset(&seraddr,0,sizeof(seraddr));
    seraddr.sin_family=AF_INET;
    seraddr.sin_port = htons(SERV_PORT);
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    Bind(server_sockfd,(struct sockaddr*)&seraddr,sizeof(seraddr));
    Listen(server_sockfd,128);

    socklen_t length = sizeof(client_addr);
    connfd = Accept(server_sockfd,(struct sockaddr*)&client_addr,&length);

    if(inet_ntop(AF_INET,&client_addr.sin_addr,clie_buff,sizeof(clie_buff))==NULL){
        perr_exit("inet_ntop error");
    }
    for (;;){
        n = Read(connfd,buff,MAXLINE);
        buff[n]='\0';
        printf("recv msg client[%s] buff : %s \n",clie_buff,buff);
        int i;
        for (i=0;i<n;i++)
            buff[i] = toupper(buff[i]);
        Write(connfd,buff,MAXLINE);
    }

}

//高并发Socket之多进程

void wait_pid(int signo){
    while (waitpid(0, NULL,WNOHANG)>0); //defunct is zombine process
    return ;
}
int Server_fork(){
    pid_t pid;
    int server_sockfd , connfd;
    struct  sockaddr_in seraddr ,client_addr;
    socklen_t  clie_addr_len;
    char buff[MAXLINE];
    char clie_buff[MAXLINE];
    int n;
    server_sockfd = Socket(AF_INET,SOCK_STREAM,0);

    memset(&seraddr,0,sizeof(seraddr));
    seraddr.sin_family=AF_INET;
    seraddr.sin_port = htons(SERV_PORT);
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    Bind(server_sockfd,(struct sockaddr*)&seraddr,sizeof(seraddr));
    Listen(server_sockfd,128);
    struct  sigaction act;
    while (1) {
        socklen_t length = sizeof(client_addr);
        connfd = Accept(server_sockfd, (struct sockaddr *) &client_addr, &length);
        if(inet_ntop(AF_INET,&client_addr.sin_addr,clie_buff,sizeof(clie_buff))==NULL){
            perr_exit("inet_ntop error");
        }
        pid = fork();
        if (pid < 0) {
            perr_exit("fork error");
        } else if (pid == 0) {  //child
            close(server_sockfd);
            break;
        } else{  //parent
            close(connfd);
            act.sa_handler = wait_pid;
            sigemptyset(&act.sa_mask);
            act.sa_flags = 0;
            if (sigaction(SIGCHLD,&act,NULL)<0){
                perr_exit("sigaction err \n");
            }
            //signal(SIGCHLD,wait_pid);
        }
    }
    if (pid == 0){
        for (;;){
            n = Read(connfd,buff,MAXLINE);
            if (n == 0){
                close(connfd);
                return 0;
            } else if (n == -1){
                perr_exit("read error");
            } else{
                buff[n]='\0';
                printf("recv msg for client[%s] buff : %s \n",clie_buff,buff);
                int i;
                for (i=0;i<n;i++)
                    buff[i] = toupper(buff[i]);
                printf("write msg to client[%s] buff : %s \n",clie_buff,buff);
                Write(connfd,buff,MAXLINE);
            }
        }
    }


}


/*select*/
int Server_select(){
    int i , j , n ,maxi;
    int nready,client[FD_SETSIZE];
    int maxfd,listenfd,connfd,sockfd;
    char buff[BUFSIZ],str[INET_ADDRSTRLEN];
    struct  sockaddr_in seraddr ,client_addr;
    socklen_t  clie_addr_len;
    fd_set rset,allset;

    listenfd = Socket(AF_INET,SOCK_STREAM,0);

    memset(&seraddr,0,sizeof(seraddr));
    seraddr.sin_family=AF_INET;
    seraddr.sin_port = htons(SERV_PORT);
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    int opt = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt,(socklen_t)sizeof(opt));
    Bind(listenfd,(struct sockaddr*)&seraddr,sizeof(seraddr));
    Listen(listenfd,20);
    maxfd = listenfd;

    maxi=-1;
    for (i=0;i<FD_SETSIZE;i++)
        client[i] = -1;
    FD_ZERO(&allset);
    FD_ZERO(&rset);
    FD_SET(listenfd,&allset);
    while(1){
        rset = allset;
        nready = select(maxfd+1,&rset,NULL,NULL,NULL);
        if (nready < 0){
            perr_exit("select error");
        } else{
            //printf("nread = %d \n",nready);
        }
        if (FD_ISSET(listenfd,&rset)){ //have a client apply to connect ,get the connect and add to the allset
            clie_addr_len = sizeof(client_addr);
            connfd = Accept(listenfd,(struct sockaddr *)&client_addr,&clie_addr_len);
            printf("receive from %s at port %d \n",
                   inet_ntop(AF_INET,&client_addr.sin_addr,str, sizeof(str)),
                   ntohs(client_addr.sin_port));

            for (i =0;i<FD_SETSIZE;i++){
                if (client[i] < 0){

                    printf("client[%d] is %d \n",i,connfd);
                    client[i] = connfd;
                    break;
                }
            }
            if ( i == FD_SETSIZE){
                fputs("too many clients  \n",stderr);
                exit(1);
            }

            FD_SET(connfd,&allset);
            if (maxfd < connfd)
                maxfd= connfd;
            if (maxi < i){
                maxi = i;
            }
            if (--nready == 0)
                continue;
        }
        for (i =0;i<=maxi;i++){
            sockfd = client[i];
            if (sockfd<0)
                continue;
            if (FD_ISSET(sockfd,&rset)){
                n=Read(sockfd,buff, sizeof(buff));
                if (n ==0){
                    close(sockfd);
                    FD_CLR(sockfd,&rset);
                    client[i]=-1;
                } else{
                    //buff[n]='\0';
                    printf("recv msg for client buff : %s \n",buff);
                    int i;
                    for (i=0;i<n;i++)
                        buff[i] = toupper(buff[i]);
                    printf("write msg to client buff : %s \n",buff);
                    Write(sockfd,buff, sizeof(buff));
                }
                if (--nready == 0)
                    break;
            }

        }

    }
    close(listenfd);
    return 0;
}



int main() {


   // Server_toupper();
   // Server_fork();
   Server_select();

    return 0;
}
