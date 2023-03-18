#include "EventLoopThread.h"
#include <mynet/EventLoop.h>
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name):loop_(nullptr),name_(name),
exiting_(false),
callback_(cb)
{
    
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if(loop_ != NULL){
        loop_ -> quit();     
    }
    if(thread_.joinable()){//线程对象析构之前 如果是joinable()状态 主程序会直接terminate退出
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop() //将这个线程中的EventlLoop对象返回出去
{   
    thread_ = std::move(std::thread(std::bind(&EventLoopThread::threadFunc,this)));

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lc(mutex_);
        cv_.wait(lc,[this](){
            return this->loop_ != nullptr;
        });
        loop = loop_;
    }
    return loop;
}

void EventLoopThread::threadFunc() //创造EventLoopThread对象，新线程默认执行的函数
{
    EventLoop loop; //在这个线程中创造一个EventLoop对象
    if(callback_){
        callback_(&loop);
    }
    {
        std::lock_guard<std::mutex> lc(mutex_);
        loop_ = &loop; //loop_是主线程的变量 ；它指向的值如果不为nullptr 是EventLoop线程中的地址
        cv_.notify_one();     
    }

    loop.loop(); //一直循环监听io

    std::lock_guard<std::mutex> lc(mutex_);
    loop_ = nullptr;
}
