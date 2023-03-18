#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include "base/Noncopyable.h"
#include "base/Timestamp.h"
using namespace std;
class EventLoop;
/**
 * Channel 是 selectable IO channel，负责注册与响应 IO 事件，它对应了一个fd的相关响应操作，但不拥有这个fd的生命周期。
 * 它是 Acceptor、Connector、EventLoop、TimerQueue、TcpConnection 的成员，生命期由后者控制。
 * 它和fd是一一对应的关系。尽管Channel不拥有fd，fd须要响应哪些事件都保存在Channel中。
 * 且有相应的回调函数。
 */
class Channel // 被EventLoop聚合
{
public:
    typedef function<void()> EventCallback;
    typedef function<void(Timestamp)> ReadEventCallback;

private:
    static string eventsToString(int fd, int e);

    void update();
    void handleEventWithGurad(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int KWriteEvent;

    EventLoop *loop_; // 指向拥有这个channel的eventloop
    const int fd_;    // 文件描述符 但不拥有这个fd的生命周期
    int events_;      // 关注的事件
    int revents_;     // it's the receievd event types of epoll or poll
    int index_;       // used by pollers;表示在poll的事件数组中的序号
    bool logHup_;     // for POLLHUP 对方描述符挂起事件

    std::weak_ptr<void> tie_;//
    bool tied_;
    bool eventHandling_; // 是否处于事件当中
    bool addedToLoop_;   // Channel的文件描述符fd，或者说是否被添加进关注列表
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

public:
    Channel(EventLoop *loop, int fd); // 一个EventLoop包含多个channel 一个channel只能所属一个EventLoop
    ~Channel();

    void handleEvent(Timestamp receivetime);
    /**一些回调函数的注册*/
    void setReadCallback(ReadEventCallback cb) { readCallback_ = move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = move(cb); }

    void tie(const shared_ptr<void> &);

    // Channel对应的文件描述符
    int fd() const
    {
        return fd_;
    }
    int events() const
    {
        return events_;
    }
    void set_revents(int revt)
    { // use by pollers
        revents_ = revt;
    }
    bool isNoEvent() const
    {
        return events_ == kNoneEvent;
    }
    //关注fd的可写事件
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    //关注fd的可读事件
    void enableWriting()
    {
        events_ |= KWriteEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~KWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }
    //是否还在关注写事件
    bool isWriting() const
    {
        return events_ & KWriteEvent;
    }
    bool isReading() const { return events_ & kReadEvent; }

    // for Poller
    int index()
    {
        return index_;
    }
    void set_index(int idx)
    {
        index_ = idx;
    }
    // for debug info
    string reventsToString() const;
    string eventsToString() const;

    void doNotLogHup()
    {
        logHup_ = false;
    }
    EventLoop *ownerLoop()
    {
        return loop_;
    }
    void remove();
};
