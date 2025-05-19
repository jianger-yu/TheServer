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
    //注册捕捉信号
    struct sigaction act;
    act.sa_handler = sig_catch;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, NULL);
    clit_addr_len = sizeof(struct sockaddr);
    while(1){
        do
        cfd = accept(fd ,(struct sockaddr*)&addr, &clit_addr_len);
        while(cfd == -1 && errno == EINTR);
        if(cfd == -1)
        sys_err("accept");
    
        printf("connect success\n");
        pid = fork();
        if(pid == 0){
            close(fd);
            while(1){
                int ret = read(cfd, buf, sizeof(buf));
                ret = sprintf(buf2,"is read ok: %s",buf);
                buf2[ret] = '\0';
                for(int i = 0; i < ret ; i ++) printf("%c",buf2[i]);
                //write( STDOUT_FILENO, buf2, ret);
                //write( cfd, buf2, ret);
                if (write(cfd, buf2, ret) == -1) {
                    perror("write error");
                    exit(1);
                }
                memset(buf,0,sizeof buf);
                memset(buf2,0,sizeof buf2);
            }
        }
        else{
            close(cfd);
            continue;
        }
    }
    
    close(fd);
    close(cfd);
    return 0;
}