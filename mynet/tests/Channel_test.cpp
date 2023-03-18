#include "base/Logger.h"
#include "mynet/Channel.h"
#include "mynet/EventLoop.h"

#include <functional>
#include <map>
#include <sstream>
#include <chrono>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <thread>
#include <sys/timerfd.h> // 将时间点转化为fd事件，时间点未到达时fd阻塞 时间点到达是fd可读
// muduo将定时器任务通过转变为fd的方式注册到EventLoop中，完成了形式上的统一

using TimerCallback = std::function<void()>;
int createTimerfd_()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        LOG_SYSFATAL << "Failed int timerfd_create";
    }
    return timerfd;
}

void readTimerfd_(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = read(timerfd, &howmany, sizeof howmany);
    LOG_TRACE << "TimerQueue::handleRead()" << howmany << " at " << now.toString();
    if (n != sizeof howmany)
    {
        LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
    }
}

void print(const char *msg)
{
    static std::map<const char *, Timestamp> lasts;
    Timestamp &last = lasts[msg];
    Timestamp now = std::chrono::system_clock::now();
    printf("%s tid %lld %s delay %lf \n",now.toString().c_str() , this_thread::get_id(),msg, timeDifference(now, last));
}


class PeriodicTimer
{
private:
    EventLoop *loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    const double interval_; // 单位s
    TimerCallback cb_;

    void handleRead()
    {
        loop_->assertInLoopThread();
        readTimerfd_(timerfd_, std::chrono::system_clock::now());
        if (cb_)
        {
            cb_();
        }
    }

    static timespec toTimeSpec(double seconds)
    {
        struct timespec ts;
        bzero(&ts, sizeof ts);
        const int64_t kNanoSecondsPerSecond = 1000000000;
        const int kMinInterval = 100000;
        int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);
        if (nanoseconds < kMinInterval)
            nanoseconds = kMinInterval;
        ts.tv_sec = static_cast<time_t>(nanoseconds / kNanoSecondsPerSecond);
        ts.tv_nsec = static_cast<long>(nanoseconds % kNanoSecondsPerSecond);
        return ts;
    }

public:
    PeriodicTimer(EventLoop *loop, double interval, const TimerCallback &cb) : loop_(loop),interval_(interval),
                                                                               timerfd_(createTimerfd_()),
                                                                               timerfdChannel_(loop_, timerfd_),
                                                                               
                                                                               cb_(cb)
    {
        timerfdChannel_.setReadCallback(std::bind(&PeriodicTimer::handleRead, this));
        timerfdChannel_.enableReading();
    }

    void start()
    {
        struct itimerspec spec;
        bzero(&spec, sizeof spec);
        spec.it_interval = toTimeSpec(interval_);
        spec.it_value = spec.it_interval;

        int ret = ::timerfd_settime(timerfd_, 0 /* relative timer */, &spec, NULL); //设定事件
        if(ret){
             LOG_SYSERR << "timerfd_settime()";
        }
    }

    ~PeriodicTimer()
    {
        timerfdChannel_.disableAll();
        timerfdChannel_.remove();
        ::close(timerfd_);
    }
};


int main(int argc, char* argv[])
{  
    std::ostringstream ss;
    ss << this_thread::get_id();
   
  LOG_INFO << "pid = " << getpid() << ", tid = " << ss.str()
           << " Try adjusting the wall clock, see what happens.";
  EventLoop loop;
  PeriodicTimer timer(&loop, 1, std::bind(print, "PeriodicTimer"));
  timer.start();
  loop.runEvery(1, std::bind(print, "EventLoop::runEvery"));
  loop.loop();
}


  