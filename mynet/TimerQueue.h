#pragma once
/**
 * 高效的定时器队列
 *
 */
#include <vector>
#include <set>
#include "base/Noncopyable.h"
// #include <base/TImestamp.h>
#include "mynet/Callbacks.h"
#include "mynet/Channel.h"
#include "atomic"
class EventLoop;
class Timer;
class TimerId;
class Timestamp;

/**
 * 自定义的类若只给出了声明，没有给出定义。不完整类型必须通过某种方式补充完整，才能使用它们进行实例化，
 * 否则只能用于定义指针或引用，因为此时实例化的是指针或引用本身，不是base或test对象。
*/

//定时器管理类
class TimerQueue : noncopyable
{
private:
//改进点是 这里Timer *用智能指针代替 unique_ptr
    using Entry = std::pair<Timestamp, Timer *>; 
    using TimerList = std::set<Entry>; //用set是因为按时间排序 
    using ActiveTimer = std::pair<Timer *, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer *timer);   //只能在所属的线程中调用 因而不必加锁 服务器性能杀手是锁竞争 因而尽可能少用锁
    void cancelInLoop(TimerId timerId); //只能在所属的线程中调用 因而不必加锁 服务器性能杀手是锁竞争 因而尽可能少用锁

    void handleRead();
    // move out all expired timers
    std::vector<Entry> getExpired(Timestamp now); //返回超时的定时器列表
    void reset(const std::vector<Entry> &expired, Timestamp now);//对超时的定时器重置 （如果是重复的定时器）

    bool insert(Timer *timer);

    EventLoop *loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    TimerList timers_;//std::set<Entry>; 按到期时间排序
    ActiveTimerSet activeTimers_;//ActiveTimerSet ; ActiveTimer = std::pair<Timer *, int64_t>; 按定时器地址排序
    // for cancel
   
    std::atomic<bool> callingExpiredTimers_; // 过期定时器
    ActiveTimerSet cancelingTimers_; 

public:
    explicit TimerQueue(EventLoop *loop);
    ~TimerQueue();

    TimerId addTimer(TimerCallback cb,Timestamp when, double interval); //添加一个定时器 一定是线程安全的，可以跨线程调用 通常情况下被其他线程调用
    void cancel(TimerId timerid); //取消一个定时器 一定是线程安全的，可以跨线程调用 通常情况下被其他线程调用
};
