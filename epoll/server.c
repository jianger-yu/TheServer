#include <signal.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <ctype.h>
#define MAX_SIZE 5000
char buf[BUFSIZ/2];
char buf2[BUFSIZ];

void sys_err(char * s){
    perror(s);
    exit(1);
}

void *read_clit(void * cfd){
    while(1){
        int ret = read((int)cfd, buf, sizeof(buf));
        write( STDOUT_FILENO, buf, ret);
    }
    return NULL;
}

void sig_catch(int sig){
    while(waitpid(0,NULL,WNOHANG) > 0);
    return;
}

int main(){
    int lfd = 0, ret = 0, cfd, clit_addr_len = 0, pid, i;
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd == 0)
        sys_err("socket error");
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1145);              //转移port
    addr.sin_addr.s_addr = htonl(INADDR_ANY); //转移ip

    bind(lfd ,(struct sockaddr*)&addr, sizeof(addr));
    
    ret = listen(lfd, 128);
    if(ret == -1)
        sys_err("listen error");
    
    int epfd = epoll_create(MAX_SIZE);
    struct epoll_event tmp, ep[MAX_SIZE];
    tmp.events = EPOLLIN;
    tmp.data.fd = lfd;
    epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&tmp);
    while(1){
        ret = epoll_wait(epfd,ep,MAX_SIZE,-1);
        if(ret < 0)
            sys_err("epoll_wait error");
        for(i = 0; i < ret ; i ++){
            if(ep[i].data.fd == lfd){//有事件申请建立连接
                clit_addr_len = sizeof addr;
                cfd = accept(lfd, (struct sockaddr *)&addr, &clit_addr_len);
                if(cfd <= 0)
                    sys_err("accept error");
                printf("connect access\n");
                tmp.data.fd = cfd;
                tmp.events = EPOLLIN;
                epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&tmp);
            }else {
                int n = read(ep[i].data.fd,buf,sizeof buf);
                if(n == 0){//对端已经断开连接
                    epoll_ctl(epfd,EPOLL_CTL_DEL,ep[i].data.fd,NULL);
                    close(ep[i].data.fd);
                } else if( n < 0) sys_err("read error");
                else{
                    for(int j = 0; j < n ; j++){
                        buf[j] = toupper(buf[j]);
                    }
                    write(ep[i].data.fd,buf,sizeof buf);
                    write(STDOUT_FILENO,buf,sizeof buf);
                    memset(buf,0 ,sizeof buf);
                }
            }
        }
    }
    close(lfd);
    close(cfd);
    return 0;
}