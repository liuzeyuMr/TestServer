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
#include <poll.h>
#include <sys/epoll.h>
#include <wait.h>
#include "warp.h"
#define MAXLINE 4096
#define SERV_PORT 6666
#define OPEN_MAX 5000
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

int Server_poll(){
    int i , j , n ,maxi;
    int nready;
    int maxfd,listenfd,connfd,sockfd;
    char buff[BUFSIZ],str[INET_ADDRSTRLEN];
    struct pollfd client[FD_SETSIZE];
    struct  sockaddr_in seraddr ,client_addr;
    socklen_t  clie_addr_len;

    listenfd = Socket(AF_INET,SOCK_STREAM,0);

    memset(&seraddr,0,sizeof(seraddr));
    seraddr.sin_family=AF_INET;
    seraddr.sin_port = htons(SERV_PORT);
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    int opt = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt,(socklen_t)sizeof(opt));
    Bind(listenfd,(struct sockaddr*)&seraddr,sizeof(seraddr));
    Listen(listenfd,128);
    client[0].fd = listenfd;
    client[0].events = POLL_IN;
    maxi=0;
    for (i=1;i<FD_SETSIZE;i++)
        client[i].fd = -1;

    while(1){
        nready = poll(client,maxi+1,-1);
        if (nready < 0){
            perr_exit("select error");
        }
        if (client[0].revents & POLL_IN){ //have a client apply to connect ,get the connect and add to the allset
            clie_addr_len = sizeof(client_addr);
            connfd = Accept(listenfd,(struct sockaddr *)&client_addr,&clie_addr_len);
            printf("receive from %s at port %d \n",
                   inet_ntop(AF_INET,&client_addr.sin_addr,str, sizeof(str)),
                   ntohs(client_addr.sin_port));

            for (i =0;i<FD_SETSIZE;i++){
                if (client[i].fd < 0){ //save the connect fd
                    printf("client[%d] is %d \n",i,connfd);
                    client[i].fd = connfd;
                    break;
                }
            }
            if ( i == FD_SETSIZE){
                perr_exit("too many clients ");
            }

            client[i].events = POLL_IN; //新加入的FD监听读事件

            if (maxi < i){
                maxi = i;
            }
            if (--nready == 0)
                continue;
        }


        for (i =1;i<=maxi;i++){
            sockfd = client[i].fd;
            if (sockfd<0) //这里还是会导致循环多次
                continue;
            if (client[i].revents &POLL_IN){
                n=Read(sockfd,buff, sizeof(buff));
                if(n < 0){
                    if (errno == ECONNRESET){
                        printf("client [%d] aborted connection \n",i);
                        Close(sockfd);
                        client[i].fd = -1;
                    }
                }
                else if (n ==0){
                    printf("client[%d] closed connection \n",i);
                    close(sockfd);
                    client[i].fd=-1;
                } else{
                    //buff[n]='\0';
                    printf("recv msg for client buff : %s \n",buff);
                    //int i;
                    for (j=0;j<n;j++)
                        buff[j] = toupper(buff[j]);
                    printf("write msg to client buff : %s \n",buff);
                    Write(sockfd,buff, sizeof(buff));
                }
                if (--nready <= 0)
                    break;
            }

        }

    }
    close(listenfd);
    return 0;
}

int Server_epoll()
{
    int listenfd , connfd ,socketfd, epfd , nready;

    struct epoll_event evt ,evts[OPEN_MAX];
    char buff[MAXLINE];
    struct sockaddr_in seraddr , client;
    socklen_t clientlen;
    int res;
    memset(&seraddr,0,sizeof(seraddr));
    seraddr.sin_family=AF_INET;
    seraddr.sin_port = htons(SERV_PORT);
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    listenfd = Socket(AF_INET,SOCK_STREAM,0);
    int opt = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt, sizeof(opt));

    Bind(listenfd,(struct sockaddr*)&seraddr, sizeof(seraddr));
    Listen(listenfd , 20);//不要忘记设置listen文件描述符
    epfd = epoll_create(10);
    if (epfd == -1){
        perr_exit("epoll_create error");
    }
    evt.events = EPOLLIN;
    evt.data.fd = listenfd;
    res = epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&evt);
    if (res == -1){
        perr_exit("epoll_ctl error");
    }
    while (1){
        nready = epoll_wait(epfd,evts,OPEN_MAX,-1);
        if (nready == -1){
            perr_exit("epoll_wait error");
        }
        int n;
        for (n=0;n<nready;n++){
            if (!(evts[n].events & EPOLLIN))
                continue;
            if (evts[n].data.fd == listenfd){ // 进行fd的比较
                clientlen = sizeof(client);
                connfd = Accept(listenfd,(struct sockaddr*)&client,&clientlen);
                evt.events = EPOLLIN;
                evt.data.fd =connfd;
                int res;
                res = epoll_ctl(epfd,EPOLL_CTL_ADD,connfd,&evt);
                if (res == -1){
                    perr_exit("epoll_ctl error");
                }
            } else{
                socketfd = evts[n].data.fd;

                n=Read(socketfd,buff, sizeof(buff));
                if(n < 0){
                    if (errno == ECONNRESET){
                        printf("client aborted connection \n");
                         int res = epoll_ctl(epfd,EPOLL_CTL_DEL,socketfd,NULL);
                         close(socketfd);
                        if (res == -1){
                            perr_exit("epoll_ctl error");
                        }
                    }
                }
                else if (n ==0){
                    printf("client closed connection \n");
                    int res = epoll_ctl(epfd,EPOLL_CTL_DEL,socketfd,NULL);
                    close(socketfd);
                    if (res == -1){
                        perr_exit("epoll_ctl error");
                    }
                } else{
                    //buff[n]='\0';
                    printf("recv msg for client buff : %s \n",buff);
                    int j;
                    for (j=0;j<n;j++)
                        buff[j] = toupper(buff[j]);
                    printf("write msg to client buff : %s \n",buff);
                    Write(socketfd,buff, sizeof(buff));
                }


            }


        }
    }

}


int main() {


   // Server_toupper();
   // Server_fork();
   //Server_select();
   //Server_poll();
    Server_epoll();
    return 0;
}
