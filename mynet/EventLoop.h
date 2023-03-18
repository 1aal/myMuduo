#pragma once 

#include <functional>
#include <chrono>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include<string>
#include <any>
#include "base/Timestamp.h"
#include "base/Noncopyable.h"
#include "mynet/Callbacks.h"
#include "mynet/TimerId.h"


// 一个创建了EventLoop对象的线程是IO线程 
// 任何在IO线程（也就是具有EventLoop对象的线程）中绑定的Channel中发生的IO事件，最终都会在EventLoop中的handleEvent环节依次处理。
//  理解这一点非常重要，如果出问题需要debug，基本上都是在handleEvent环节中有线程安全的问题。
//Eventloop是Reactor模式的封装 每个线程最多一个Eventloop对象
//Eventloop  什么都不做 功能就是运行事件循环
class Channel;
class Poller;
class TimerQueue;

class EventLoop : noncopyable
{
public:
    typedef std::function<void()> Functor;
   
    void loop(); // 必须在产生调用这个loop()的EventLoop对象的线程里执行

    void quit(); // 可以跨线程调用quit

    Timestamp pollReturnTime() const { return pollReturnTime_; } //poll()的返回时间

    int64_t iteration() const;

    void runInLoop(Functor cb); //让IO线程也能完成一定的计算任务 通过调用queueInLoop 用队列存储实现线程安全的异步调用
    void queueInLoop(Functor cb); //cb加入IO线程的任务队列 

    size_t queueSize() const;


    //**timers

    //run cb at time ,safe to call from other threads
    TimerId runAt(Timestamp time,TimerCallback cb); //在某个时刻运行定时器
    //run cb after @c delay seconds ,safe to call from other threads
    TimerId runAfter(double delay,TimerCallback cb);//过一段时间运行定时器
    //run cb after every @c interval seconds ,safe to call from other threads
    TimerId runEvery(double interval, TimerCallback cb);//每隔一段时间运行定时器

    void cancel(TimerId tiemrId); //取消定时器


    void assertInLoopThread();
    bool isInLoopThread() const;
    bool eventHandling() const;

    //context
    
    void setContext(const std::any& context){
        context_ = context;
    }

    const std::any& getContext() const{
        return context_;
    }

    std::any& getMutableContext(){
        return context_;
    }


    static EventLoop *getEventLoopOfCurrentThread();

    // internal usage
    void wakeup();
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);


private:
    void abortNotInLoopThread();
    void handleRead(); //wakeup
    void doPendingFunctors(); //执行 其他线程或者本线程添加的一些IO任务

    void printActiveChannels() const; // for debug

    typedef std::vector<Channel *> ChannelList;

    bool looping_; /**原子操作*/
    std::atomic<bool> quit_;
    bool eventHandling_;          // atomic;
    bool callingPendingFunctors_; // atomic;
    int64_t iteration_; //loop轮询次数
    // const std::string threadId_; //c++标准库的thread id过长 直接按字符处理；注意在多进程中可能会有两个相同的线程id
    const std::thread::id threadId_;
    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerqueue_;
    int wakeupFd_;  //由于eventfd
    std::unique_ptr<Channel> wakeupChannel_;
    std::any context_; //c++17

    ChannelList activeChannels_;
    Channel *currentActiveChannel_; //当前正在处理的channel

    mutable std::mutex mutex_;
    std::vector<Functor> pendingFunctors_; //IO线程的计算任务队列 大部分都是其他线程添加进来的任务

public:
    EventLoop(/* args */);
    ~EventLoop();
};
