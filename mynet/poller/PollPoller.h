#pragma once 
#include "mynet/Poller.h"
#include <vector>

struct pollfd;
class PollPoller :public Poller //默认是private继承 基类中的所有成员在子类中都为private 
{

public:
    PollPoller(EventLoop* loop);
    ~PollPoller() override;
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;   

private:
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    typedef std::vector<pollfd> PollFdList; //poll()操作每次要在内核与用户间传递的所有pollfd对象数组 当channel被移除时需要手动移除这个数组里对应的pollfd对象
    PollFdList pollfds_;
};
