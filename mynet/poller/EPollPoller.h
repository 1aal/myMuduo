#pragma once 
#include "mynet/Poller.h"

struct epoll_event;

class EPollPoller :public Poller
{
private:
    /* data */
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    void update(int operation,Channel* channel);
    typedef std::vector<epoll_event> EventList; //epoll每次返回的被触发的事件数组,这里全是已触发的事件；channel被移除时不需要手动修改EventList里的事件
    //  所以channel对象在应用到EpollPoll对象的时候 index的实现十分简单 不需要记录在EventList里的位置 只需要标记是否时新加入 已加入 和 删除
    int epollfd_;
    EventList events_;

    static const int kInitEventListSize = 16;
    static const char* operationToString(int op);
};

