#include "PollPoller.h"

#include <assert.h>
#include <poll.h>
#include "base/Logger.h"
#include "mynet/Channel.h"

PollPoller::PollPoller(EventLoop *loop) : Poller(loop)
{
}

PollPoller::~PollPoller() = default;
//用了某个类中声明的typedef 用这个类做返回值的时候 要声明具体的类
Timestamp PollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
    int savedErron = errno;
    Timestamp now = std::chrono::system_clock::now();
    if (numEvents > 0)
    {
        LOG_TRACE << numEvents << " events happend";
        fillActiveChannels(numEvents, activeChannels);
    }
    else if (numEvents == 0)
    {
        LOG_TRACE << "poll receive nothing...";
    }
    else
    {
        if (savedErron != EINTR)
        {
            errno = savedErron;
            LOG_SYSERR << "PollPoller::poll()";
        }
    }
    return now;
}
void PollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{  
    for(auto it = pollfds_.begin(); it!= pollfds_.end() && numEvents > 0 ; ++it ){
        if (it->revents > 0)
        {
            --numEvents;
            auto ch = channels_.find(it->fd);
            assert(ch != channels_.end());
            Channel *channel = ch->second;
            assert(channel->fd() == it->fd);
            channel->set_revents(it->revents);
            activeChannels->push_back(channel);
        }
        
    }
}

void PollPoller::updateChannel(Channel *channel)
{
    Poller::assertInLoopThread();
    LOG_TRACE<<"fd = "<<channel->fd()<< " events = "<<channel->events();
    if(channel->index() < 0){ //表示此时的channel还没有加入过PollPoller 因为Channel构造函数的默认值index = -1
        //add new one
        assert(channels_.find(channel->fd()) == channels_.end());
        pollfd pfd;
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;//待poll()返回的events信息
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size())-1;
        channel ->set_index(idx);
        channels_[pfd.fd] = channel;
        //channels_是map,在基类中声明 用来存放PollPoller对象已经联系起来的channel
        //pollfds_是vector 存放的是pollfd类型数组，可以当作::poll()函数的第一个参数
    }else{
        //update existing one
        assert(channels_.find(channel->fd()) != channels_.end());
        assert(channels_[channel->fd()] == channel);
        int idx = channel ->index();
        assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

        pollfd& pfd = pollfds_[idx]; //引用
        assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1); //pfd中的fd小于0表示不关注这个pfd    
        pfd.fd = channel->fd();
        pfd.events =static_cast<short>(channel->events());
        pfd.revents = 0;  
        if(channel->isNoEvent()){  //如果当前channel代表的fd是不被关注的
            pfd.fd = -channel->fd()-1;
        }

    }
}

void PollPoller::removeChannel(Channel *channel)
{
    Poller::assertInLoopThread();
    LOG_TRACE << "fd = "<< channel->fd();
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    assert(channel->isNoEvent());//被移除的channel必须是不被关注的 
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

    size_t n = channels_.erase(channel->fd());//从map中移除 这个channel; 返回删除的个数
    assert (n == 1);
    //从pollfds_中删除channel代表的fd 采用O(1)的算法: 与最后一个元素交换位置 并pop_back()
    if(pollfds_.size()-1 == idx){
        pollfds_.pop_back();
    }else{
        int ChannelFdAtEnd = pollfds_.back().fd; //pollfds_中的fd可能是负的 因为被置为不被关注的了
        iter_swap(pollfds_.begin() + idx,pollfds_.end()-1); //交换channel的迭代器和最后一个迭代器的内容
        if(ChannelFdAtEnd < 0){
            ChannelFdAtEnd = -ChannelFdAtEnd -1;
        }
        channels_[ChannelFdAtEnd]->set_index(idx);
        pollfds_.pop_back();
    }

}


