#pragma once
#include "mynet/Callbacks.h"
#include "base/Timestamp.h"
#include "base/Noncopyable.h"
#include <atomic>

class Timer :noncopyable //定时器高层次的封装类 封装了 定时器任务的各项接口
{
private:
    const TimerCallback callback_;
    Timestamp expiration_; //超时时刻
    const double interval_;  //超时事件间隔 如果单次定时器这里为0
    const bool repeat_;  
    const int64_t sequence_; //定时器序号
    static std::atomic<int64_t> s_numCreated_; //当前已创造的定时器数量
    
public:
    Timer(TimerCallback cb, Timestamp when, double interval):callback_(std::move(cb)),
    expiration_(when),
    interval_(interval),
    repeat_(interval_ > 0.0),
    sequence_(++s_numCreated_)
    {}
    ~Timer() = default;

    void run(){
        callback_();
    }
    Timestamp expiration() const{
        return expiration_;
    }
    bool repeat() const{
        return repeat_;
    }
    int64_t sequence() const{
        return sequence_;
    }

    void restart(Timestamp now);
    
    static int64_t numCreate(){
        return s_numCreated_.load();
    }
};



