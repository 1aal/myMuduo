#include "Poller.h"
#include "mynet/EventLoop.h"
#include "mynet/Channel.h"
Poller::Poller(EventLoop *loop):ownerLoop_(loop)
{
}

Poller::~Poller() = default;


bool Poller::hasChannel(Channel* channel) const
{
    assertInLoopThread();
    ChannelMap::const_iterator it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

// Poller *Poller::newDefualtPoller(EventLoop *loop)
// {
//     return nullptr;
// }

void Poller::assertInLoopThread() const
{
    ownerLoop_->assertInLoopThread();
}
