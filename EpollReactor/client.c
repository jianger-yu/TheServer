#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
char buf[BUFSIZ];

void sys_err(char * s){
    perror(s);
    exit(1);
}
void *read_clit(void * fd){
    while(1){
        int ret = read((int)fd, buf, sizeof buf);
        write( STDOUT_FILENO, buf, ret);
    }
    return NULL;
}
/*

void *read_clit(void *fd) {
    int total = 0, ret;
    while (1) {
        ret = recv((int)fd, buf + total, sizeof(buf) - total, 0);
        if (ret <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue; // 非阻塞模式下需处理
            perror("recv error");
            break;
        }
        total += ret;
        if (total >= sizeof(buf) || buf[total-1] == '\n') { // 按需判断结束条件
            write(STDOUT_FILENO, buf, total);
            total = 0;
        }
    }
    return NULL;
}
*/

int main(){
    int fd = 0, ret = 0;
    fd = socket( AF_INET, SOCK_STREAM, 0);
    if(fd == -1)
        sys_err("socket error");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1145);

    ret = inet_pton( AF_INET,"127.0.0.1",&addr.sin_addr.s_addr);
    if(ret < 0)
        sys_err("inet_pton error");
    else if (ret == 0) {
        fprintf(stderr, "Invalid IP address format\n");
        exit(1);
    }
    
    ret = connect( fd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret == -1)
        sys_err("connect error");
    printf("connect success\n");
    pthread_t tid = 0;
    pthread_create(&tid, NULL, read_clit,(void *)fd);

    while(1){
        memset(buf,0,sizeof buf);
        ret = read(STDIN_FILENO,buf,sizeof(buf));
        write( fd, buf, ret);
    }
    close(fd);
    return 0;
}