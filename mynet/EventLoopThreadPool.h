#pragma once

#include "base/Noncopyable.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    EventLoopThreadPool(EventLoop *baseloop,const std::string & nameArg);
    ~EventLoopThreadPool();
    void setThreadNum(int numThread){numThread_ = numThread;}
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    EventLoop* getNextLoop();
    EventLoop* getLoopForHash(size_t hashcode);

    std::vector<EventLoop*> getAllLoops();

    bool started() const{
        return started_;
    }

    const std::string& name()  const{
        return name_;
    }
private:
    EventLoop *baseLoop_;
    std::string name_;
    bool started_;
    int numThread_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};


