#pragma once 
/**Poller类是I/O多路复用接口（抽象）类，对I/O多路复用API的封装
 * Pooler记录监控了哪些 fd，和每个 fd 所对应的 Channel（由updateChannel，removeChannel设置）
 * 调用 poll(int timeoutMs, ChannelList* activeChannels)，
 * 找的被监控的所有 fd 中其所关心的事件发生了的 fd 所对应的 Channel，
 * 将发生了的事件通过调用 Channel::set_revents 记录在该 Channel 中，并返回这些 Channel
*/
#include <vector>
#include <map>
#include <chrono>
#include "base/Noncopyable.h"
#include "base/Timestamp.h"

class Channel;
class EventLoop;

class Poller : noncopyable
{
public:
    typedef std::vector<Channel*> ChannelList; //Channel的index_用于这个数组 这个数组在中Poller的子类pollpoller/epoller中声明
    
    Poller(EventLoop *loop);
    virtual ~Poller();

    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *Channel) = 0;
    virtual void removeChannel(Channel *Channel) = 0;
    virtual bool hasChannel(Channel *Channel) const;

    
    void assertInLoopThread() const;

    static Poller* newDefualtPoller(EventLoop* loop);
protected:
    typedef std::map<int, Channel *> ChannelMap;
    ChannelMap channels_; //每一个Poller对象 可以有多个channel对象，通过每个channel对象拥有的fd来标识不同的channel

private:
    EventLoop* ownerLoop_; //拥有Poll的EventLoop对象的指针
};
