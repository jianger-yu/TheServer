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
#include <sys/select.h>
#include <ctype.h>

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
    int fd = 0, ret = 0, cfd, clit_addr_len = 0, pid;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == 0)
        sys_err("socket error");
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1145);              //转移port
    addr.sin_addr.s_addr = htonl(INADDR_ANY); //转移ip

    bind(fd ,(struct sockaddr*)&addr, sizeof(addr));
    
    ret = listen(fd, 128);
    if(ret == -1)
        sys_err("listen error");
    //
    
    fd_set rset, allset;
    int maxfd = fd,addr_len;
    FD_ZERO(&allset);
    FD_SET(fd, &allset);

    while(1){
        rset = allset;
        ret = select(maxfd + 1, &rset, NULL, NULL,NULL);
        if(ret < 0)
            sys_err("select error");
        if(FD_ISSET(fd, &rset)){
            addr_len = sizeof(addr);
            cfd = accept(fd, (struct sockaddr *)&addr,&addr_len);
            if(cfd < 0) sys_err("accept error");
            
            FD_SET(cfd, &allset);
            if(maxfd < cfd)
                maxfd = cfd;
            if(ret == 1)    //select只返回一个，且是listen
                continue;
        }
        for(int i = fd + 1; i <= maxfd; i++){
            if(FD_ISSET(i, &rset)){//进行读的处理
                int n = read(i,buf,sizeof buf);
                if(n == 0){
                    close(i);
                    FD_CLR(i, &allset);
                } 
                else if (n == -1)
                    sys_err("read error");
                for(int j = 0; j < n; j++){
                    buf[j] = toupper(buf[j]);
                }
                write(i,buf,n);
                write(STDOUT_FILENO,buf,n);
            }
        }

    }
    close(fd);
    close(cfd);
    return 0;
}