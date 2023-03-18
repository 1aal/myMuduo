/**
 * 定时器部分测试 
*/

#include "mynet/EventLoop.h"
#include "mynet/Channel.h"

#include <unistd.h>
#include <sys/timerfd.h>
#include <string.h>

int timerfd;
EventLoop * g_loop;
void timeout(Timestamp receiveTime){
    printf("Time Out!\n");
    uint64_t howmany;
    read(timerfd,&howmany,sizeof howmany);
}


int main(){
    EventLoop loop;
    g_loop = &loop;

    timerfd = ::timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK|TFD_CLOEXEC);
    Channel channle(&loop, timerfd);
    channle.setReadCallback(std::bind(timeout,std::placeholders::_1));
    channle.enableReading();

    itimerspec howlong;
    bzero(&howlong,sizeof howlong);
    howlong.it_value.tv_sec = 1;
    howlong.it_interval.tv_sec = 1;
    timerfd_settime(timerfd,0,&howlong,NULL); //sys函数 设置
    /*
    struct itimerspec
  {
    struct timespec it_interval; //单次定时器这里设为0
    struct timespec it_value; //下次触发的事件
  };*/
    loop.loop();
    close(timerfd);

}