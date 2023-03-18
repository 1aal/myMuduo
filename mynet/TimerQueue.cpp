#include "TimerQueue.h"
#include "base/Logger.h"
#include "mynet/EventLoop.h"
#include "mynet/Timer.h"
#include "mynet/TimerId.h"
#include <base/Timestamp.h>
#include <assert.h>
#include <sys/timerfd.h>
#include <unistd.h>

int createTimerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        LOG_SYSFATAL << "Failed int timerfd_create";
    }
    return timerfd;
}

timespec howMuchTimeFromNow(Timestamp when) // timespec linux中记录的秒结构 设置超时时间的关键函数 这里错了会影响超时的设置
{
    int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100)
    {
        microseconds = 100;
    }
    timespec ts;
    ts.tv_sec =  static_cast<time_t>(microseconds /Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = read(timerfd, &howmany, sizeof howmany);
    LOG_TRACE << "TimerQueue::handleRead()" << howmany << " at " << now.toString();
    if (n != sizeof howmany)
    {
        LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
    }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
    // POSIX.1b structure for timer start values and intervals.
    itimerspec newValue;
    itimerspec oldValue;
    bzero(&newValue, sizeof newValue);
    bzero(&oldValue, sizeof oldValue);

    newValue.it_value = howMuchTimeFromNow(expiration); //计算超时时刻与当前时刻的时间差
    int ret = timerfd_settime(timerfd, 0, &newValue, &oldValue);//新的到期时间 和 获取到之前设定的到期时间
  
    if (ret)
    {
        LOG_SYSERR << "timerfd_settime()";
    }
}

TimerQueue::TimerQueue(EventLoop *loop) : loop_(loop),
                                          timerfd_(createTimerfd()),
                                          timerfdChannel_(loop_, timerfd_),
                                          timers_(),
                                          callingExpiredTimers_(false)
{   
    LOG_TRACE << "timerfd_ is "<<timerfd_;
    timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
    timerfdChannel_.enableReading(); //调用channel的update() -> 调用EventLoop的UpdateChannel() 
    
}

TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    close(timerfd_);
    for (const Entry &tiemr : timers_)
    {
        delete tiemr.second;
    }
}

TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)//回调函数 超时时间 间隔时间
{
    Timer* timer = new Timer(std::move(cb),when,interval);
    loop_->runInLoop(bind(&TimerQueue::addTimerInLoop,this,timer)); //实现安全的跨线程调用 如果TimerId和EventLoop不是在同一个线程 则将addTimerInLoop这个cb加入EventLoop的线程的任务队列
    return TimerId(timer,timer->sequence());
}

void TimerQueue::cancel(TimerId timerid)
{
    loop_->runInLoop(bind(&TimerQueue::cancelInLoop,this,timerid)); //线程安全
}

void TimerQueue::addTimerInLoop(Timer *timer)
{
    loop_->assertInLoopThread();
 //插入一个定时器 可能会使最早到期的定时器发生改变
    bool earliestChanged = insert(timer);
    if (earliestChanged)
    {   //如果最早到期的定时器发生改变 则重置定时器的超时时刻
        resetTimerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    if (it != activeTimers_.end()) //能找到 
    {
        int n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1);
        delete it->first;
        activeTimers_.erase(it);
    }
    else if (callingExpiredTimers_)
    {   //已经到期 并且正则调用回调函数
        cancelingTimers_.insert(timer); //插入cancelingTimers_ reset时会判断不需要再重启了
    }
    assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead() //定时器fd被poll/epoll响应后 会回调的函数
{   
    loop_->assertInLoopThread();
    Timestamp now = std::chrono::system_clock::now();
    readTimerfd(timerfd_, now); //清除定时器 避免一直触发
    std::vector<Entry> expired = getExpired(now); // 获得该时刻之前的 所有的定时器列表（即超时定时器列表）
    //只关注了第一个定时器超时 但是可能会有很多定时器超时
    callingExpiredTimers_ = true; //处理超时的定时器
    cancelingTimers_.clear();
    for (const auto &it : expired)//调用所有超时定时器的回调函数
    {
        it.second->run();
    }
    callingExpiredTimers_ = false;
    reset(expired, now); //如果不是一次性定时器 需要重启
}

//为什么不返回引用类型 因为rvo的优化
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;
    Entry sentry(now, reinterpret_cast<Timer *>(UINTPTR_MAX));
    TimerList::iterator end = timers_.lower_bound(sentry); //按时间排序的 返回第一个≥expired的位置 又由于sentry的pair的第二个参数地址是最大的 所以能确保lower_bound后 now < end->first
    assert(end == timers_.end() || now < end->first);
    std::copy(timers_.begin(), end, back_inserter(expired)); // 我们常常使用back_inserter来创建一个迭代器，作为算法的目的位置来使用 eg:fill_n(back_inserter(vec), 10, 0)
    
    timers_.erase(timers_.begin(), end);

    for (const auto &it : expired) //从ActiverTimers中移除到期的定时器
    {
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
    }

    assert(timers_.size() == activeTimers_.size());
    return expired;
}

void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now)
{
    Timestamp nextExpire;
    for (const auto &it : expired)
    {
        ActiveTimer timer(it.second, it.second->sequence());
        if (it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end())
        { // 是重复性计时器且还不在删除队列里
            it.second->restart(now);
            insert(it.second);
        }else{
            delete it.second; //to do 如何不频繁删除 而是 move to a free list 来减少内存分配的开销
        }
    }

    if(!timers_.empty()){
        nextExpire = timers_.begin()->second->expiration();
    }
    if(nextExpire.valid() > 0){ // nextExpire.valid() 时间合法
        resetTimerfd(timerfd_,nextExpire);
    }
}

bool TimerQueue::insert(Timer *timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    bool earliestChanged = false; //最早到期时间是否改变
    Timestamp when = timer->expiration();
    auto it = timers_.begin();
    if( it == timers_.end() || when < it->first){  //如果timers_为空或者最早到期时间的定时器发生改变
        earliestChanged = true;
    }
    {   //插入到timers_中
        pair<TimerList::iterator,bool> result = timers_.insert(Entry(when,timer));
        assert(result.second);
    }
    {   //插入到activeTimers中
        pair<ActiveTimerSet::iterator,bool> result = activeTimers_.insert(ActiveTimer(timer,timer->sequence()));
        assert(result.second);
    }
    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}
