#pragma once
/**
 * muduo中将定时器任务以fd_形式打开 计时未达到时fd_阻塞 达到时fd_解除阻塞 #include <sys/timerfd.h>
*/
#include <stdint.h>
class Timer;

class TimerId
{
private:
    Timer* timer_;   //定时器的地址
    int64_t sequence_; //定时器序号
public:
    TimerId(Timer* timer,int64_t seq):timer_(timer),sequence_(seq){};
    friend class TimerQueue;
};

