#include "EventLoopThreadPool.h"
#include "mynet/EventLoop.h"
#include "mynet/EventLoopThread.h"
#include <assert.h>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseloop, const std::string &nameArg)
    : baseLoop_(baseloop), name_(nameArg), started_(false), numThread_(0), next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{ // 不用delete loop 因为都是栈上对象
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    assert(!started_);
    baseLoop_->assertInLoopThread();

    started_ = true;
    for (int i = 0; i < numThread_; ++i)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);

        EventLoopThread *t = new EventLoopThread(cb, buf); // new出来的EventLoopThread类对象 由智能指针负责销毁  
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));//因为是unique指针 所以可以这样初始化 但也不推荐野指针和指针指针混用 不然推荐用make_share
        loops_.push_back(t->startLoop());
    }
    if (numThread_ == 0 && cb)
    {
        cb(baseLoop_);//只有一个EventLoop的话 在进入loop循环之前会调用cb_
    }
}

EventLoop *EventLoopThreadPool::getNextLoop()
{
    baseLoop_->assertInLoopThread();
    assert(started_);
    EventLoop *loop = baseLoop_;
    if (!loops_.empty())
    {
        loop = loops_[next_];
        ++next_;
        if (static_cast<size_t>(next_) >= loops_.size())
        {
            next_ = 0;
        }
    }

    return loop;
}

EventLoop *EventLoopThreadPool::getLoopForHash(size_t hashcode)
{
    baseLoop_->assertInLoopThread();
    assert(started_);
    EventLoop *loop = baseLoop_;
    if (!loops_.empty())
    {
        loop = loops_[hashcode % loops_.size()];
    }
    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    baseLoop_->assertInLoopThread();
    assert(started_);
    if(!loops_.empty()){
        return std::vector<EventLoop*>(1,baseLoop_);
    }else{
        return loops_;
    }
}


