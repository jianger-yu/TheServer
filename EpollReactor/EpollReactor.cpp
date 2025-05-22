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
#include <time.h>

#define MAX_EVENTS 1024     //监听上限数
#define BUFLEN 4096
#define SERV_PORT 1145



class readctor{
private:

typedef struct event{
    int fd;         //待监听的文件描述符
    int events;     //对应的监听事件
    void*arg;       //泛型参数
    void (readctor::*call_back)(int fd, int events, void * arg); //回调函数
    int status;     //是否在红黑树上，1->在，0->不在
    char buf[BUFLEN];
    int len;
    long last_active;   //记录最后加入红黑树的时间值
}event;

    int epfd;   //红黑树的句柄
    event r_events[MAX_EVENTS + 1];

    void eventdel(event * ev){
        struct epoll_event epv = {0,{0}};
        
        if(ev -> status != 1)  //不在红黑树上
            return;
        
        epv.data.ptr = NULL;
        ev -> status = 0;
        epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &epv);
        return ;
    }

    //监听回调
    void acceptconn(int lfd,int tmp, void * arg){
        struct sockaddr_in caddr;
        socklen_t len = sizeof caddr;
        int cfd, i;
        
        if((cfd = accept(lfd, (struct sockaddr *)&caddr,&len)) == -1){
            if(errno != EAGAIN && errno != EINTR){
                //暂时不做出错处理
            }
            printf("accept, %s\n",strerror(errno));
            return;
        }
        do{
            for(i = 0; i < MAX_EVENTS ; i ++)       //从r_events中找到一个空闲位置
                if(r_events[i].status == 0)
                    break;

            if(i == MAX_EVENTS){
                printf("max connect limit[%d]\n",MAX_EVENTS);
                break;
            }

            int flag = 0;
            if((flag = fcntl(cfd, F_SETFL, O_NONBLOCK)) < 0){       //将cfd也设置为非阻塞
                printf("fcntl nonblocking failed, %s\n",strerror(errno));
                break;
            }

            eventset(&r_events[i], cfd, &readctor::recvdata, &r_events[i]);
            eventadd(EPOLLIN, &r_events[i]);
        }while(0);

        printf("new connect [%s:%d][time:%ld], pos[%d]\n",
                inet_ntoa(caddr.sin_addr),ntohs(caddr.sin_port), r_events[i].last_active, i);
        return;
    }

    //写回调
    void senddata(int fd,int tmp, void * arg){
        event * ev = (event*)arg;
        int len;

        len = send(fd, ev->buf,ev->len,0);
        eventdel(ev);
        
        if(len > 0){
            printf("send[fd = %d],[%d]%s\n",fd,len,ev->buf);
            eventset(ev,fd,&readctor::recvdata,ev);
            eventadd(EPOLLIN, ev);
        }else {
            close(ev->fd);
            printf("send[fd = %d] , len = %d, error %s\n",fd,len,strerror(errno));
        }
    }

    
    //读回调
    void recvdata(int fd, int events, void*arg){
        event *ev = (event *) arg;
        int len;

        len = recv(fd, ev->buf, sizeof ev->buf, 0);     //读文件描述符，数据存入event的buf中
        eventdel(ev);//将该节点从红黑树摘除

        if(len > 0){
            ev->len = len;
            ev->buf[len] ='\0';
            printf("C[%d]:%s",fd,ev->buf);

            eventset(ev,fd,&readctor::senddata,ev);    //设置该fd对应的回调函数为senddata
            eventadd(EPOLLOUT, ev);         //将fd加入红黑树中，监听其写事件
        } else if(len == 0){//对端已关闭
            close(ev->fd);
            printf("[fd = %d] pos[%ld], closed\n", fd, ev-r_events);
        }else{
            close(ev->fd);
            printf("recv[fd = %d] error[%d]:%s\n",fd,errno,strerror(errno));
        }
    }

    //初始化事件
    void eventset(event * ev, int fd, void (readctor::* call_back)(int ,int , void *), void * arg){
        ev -> fd = fd;
        ev -> call_back = call_back;
        ev -> arg = arg;

        ev -> events = 0;
        ev -> status = 0; 
        ev -> last_active = time(NULL);     //调用eventset函数的时间

        return;
    }

    //添加文件描述符到红黑树
    void eventadd(int events, event * ev){
        //采用ET模式
        events |= EPOLLET;
        struct epoll_event epv = { 0 , { 0 }};
        int op;
        epv.data.ptr = ev;
        epv.events = ev -> events = events;

        if(ev -> status == 0){      //若ev已在树内
            op = EPOLL_CTL_ADD;
            ev -> status = 1;
        }
        if(epoll_ctl(epfd, op, ev -> fd, &epv) < 0)
            printf("epoll_ctl is error :[fd = %d], events[%d]\n", ev->fd, events);
        else
            printf("epoll_ctl sccess on [fd = %d], [op = %d] events[%0X]\n",ev->fd, op, events);
    }

    //初始化监听socket
    void InitListenSocket(unsigned short port){
        struct sockaddr_in addr;
        int lfd = socket(AF_INET, SOCK_STREAM, 0);
        fcntl(lfd, F_SETFL, O_NONBLOCK);

        memset(&addr, 0, sizeof addr);
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        bind(lfd,(sockaddr *)&addr, sizeof addr);

        listen(lfd, 20);

        eventset(&r_events[MAX_EVENTS], lfd, &readctor::acceptconn, &r_events[MAX_EVENTS]);
        eventadd(EPOLLIN, &r_events[MAX_EVENTS]);
    }

    

    void readctorinit(unsigned short port){
        epfd = epoll_create(MAX_EVENTS + 1);            //定义最大节点数为MAX_EVENTS + 1的红黑树
        if(epfd <= 0)
            printf("epfd create is error, epfd : %d\n", epfd);
        InitListenSocket(port);                         //初始化套接字

        struct epoll_event events[MAX_EVENTS + 1];      //保存已经满足就绪事件的文件描述符
        printf("server running port:[%d]\n", port);
        int chekckpos = 0, i;

        while(1){
            //↓↓↓超时验证
            long now = time(NULL);
            for(i = 0; i < 100; i++, chekckpos++){       //一次循环检验100个，chekckpos控制检验对象
                if(chekckpos == MAX_EVENTS)
                    chekckpos = 0;
                if(r_events[chekckpos].status != 1)      //不在红黑树上，继续循环
                    continue;
                
                long duration = now -r_events[chekckpos].last_active;   //计算客户端不活跃的时间
                if(duration >= 60){
                    printf("[fd = %d] timeout\n", r_events[chekckpos].fd);
                    eventdel(&r_events[MAX_EVENTS]);
                    close(r_events[chekckpos].fd);
                }
            }
            //↑↑↑超时验证
            //监听红黑树epfd，将满足的事件的文件描述符加至events数组中，1秒没有文件满足，则返回0
            int nfd = epoll_wait(epfd, events, MAX_EVENTS + 1, 1000); 
            if(nfd < 0){
                printf("epoll_wait error\n");
                break;
            }

            for(i = 0; i < nfd; i++){
                event *ev = (event *) events[i].data.ptr;
                //读事件，调用读回调
                if((events[i].events & EPOLLIN) && (ev -> events & EPOLLIN)){
                    (this->*ev->call_back)(ev->fd, events[i].events, ev->arg);
                }
                //写事件，调用写回调
                if((events[i].events & EPOLLOUT) && (ev -> events & EPOLLOUT)){
                    (this->*ev->call_back)(ev->fd, events[i].events, ev->arg);
                }

            }
        }

    }

public:
    // 无参构造函数
    readctor(){
        unsigned short port = SERV_PORT;
        readctorinit(port);
    }          
    // 带参构造函数
    readctor(unsigned short port){
        readctorinit(port);
    }   
};

int main(){
    readctor();
    return 0;
}