#include <poll.h>
#include <assert.h>
#include <sstream>
#include "Channel.h"
#include "mynet/EventLoop.h" //头文件之间不要互相包含 在.cpp中包含对方头文件
#include "base/Logger.h"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI; // POLLPRI 紧迫数据
const int Channel::KWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fd) : loop_(loop),
                                            fd_(fd),
                                            events_(0),
                                            revents_(0),
                                            index_(-1),
                                            logHup_(true),
                                            tied_(false),
                                            eventHandling_(false),
                                            addedToLoop_(false)
{
}

Channel::~Channel()
{
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if (loop_->isInLoopThread())
    {
        assert(!loop_->hasChannel(this));
    }
}

void Channel::handleEvent(Timestamp receivetime) // 每个channel自己的事件回调函数
{
    std::shared_ptr<void> guard; // 创立临时对象
    if (tied_)
    { ////tie_是对TcpConnection的弱引用，因为回调函数都是TcpConnection的，所以在调用之前需要确保TcpConnection没有被销毁，
        // 所以将tie_提升为shared_ptr判断TcpConnection是否还存在，之后再调用TcpConnection的一系列回调函数
        guard = tie_.lock();
        if (guard)
        {
            handleEventWithGurad(receivetime);
        }
    }
    else
    {
        handleEventWithGurad(receivetime);
    }
}

void Channel::handleEventWithGurad(Timestamp receiveTime) // handleEventWithGurad和handleEvent是有什么区别
{
    eventHandling_ = true;
    LOG_TRACE << reventsToString();
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
    {
        if (logHup_)
        {
            LOG_WARN << "fd = " << fd_ << "Channel::handle_event() POLLHUP";
        }
        if (closeCallback_)
            closeCallback_();
    }

    if (revents_ & POLLNVAL)
    {
        LOG_WARN << "fd = " << fd_ << "Channel::handle_event() POLLNVAL";
    }

    if (revents_ & (POLLERR | POLLNVAL))
    {
        if (errorCallback_)
            errorCallback_();
    }
    // 可读事件 readCallback_为Acceptor类的handleRead 或者 TcpConnection类的::handleRead
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))
    {
        if (readCallback_)
            readCallback_(receiveTime);
    }
    if (revents_ & POLLOUT) // 可写事件
    {
        if (writeCallback_)
            writeCallback_();
    }
    eventHandling_ = false;
}

void Channel::tie(const shared_ptr<void> &obj) // shared_ptr<void>表示可以引用到任意类型指针
{
    tie_ = obj;
    tied_ = true;
}

string Channel::reventsToString() const // 用于Evenlopp中的调试输出
{
    return eventsToString(fd_, revents_);
}

string Channel::eventsToString() const
{
    return eventsToString(fd_, events_);
}

string Channel::eventsToString(int fd, int ev)
{
    std::ostringstream oss;
    oss << fd << ": ";
    if (ev & POLLIN)
    {
        oss << "IN ";
    }
    if (ev & POLLPRI)
    {
        oss << "PRI ";
    }
    if (ev & POLLOUT)
    {
        oss << "OUT ";
    }
    if (ev & POLLHUP)
    {
        oss << "HUP ";
    }
    if (ev & POLLRDHUP)
    {
        oss << "RDHUP ";
    }
    if (ev & POLLERR)
    {
        oss << "ERR ";
    }
    if (ev & POLLNVAL)
    {
        oss << "NAVL ";
    }
    return oss.str();
}

void Channel::update()
{
    addedToLoop_ = true; // 标明 这个channel已被加到一个eventLoop中去
    loop_->updateChannel(this);
}

// 将channel冲loop的关注中移除
void Channel::remove()
{
    assert(isNoEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}
