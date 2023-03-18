#pragma once

#include "base/Noncopyable.h"
#include <functional>
#include <thread>
#include <string>
#include <mutex>
#include <condition_variable>
class EventLoop;
class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();
private :
    void threadFunc();
    EventLoop *loop_; //地址空间在主线程中 指向的是新创建的线程中的 EventLoop对象 即这个loop_在主线程和子线程中都可能访问到 需要加锁
    bool exiting_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;//loop指针的信号量
    
    ThreadInitCallback callback_;
    std::string name_;        
};