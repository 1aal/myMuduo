#include "EPollPoller.h"
#include "base/Logger.h"
#include "mynet/Channel.h"

#include <sys/epoll.h>
#include <poll.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

static_assert(EPOLLIN == POLLIN, "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI, "epoll uses same flag values as poll");
static_assert(EPOLLOUT == POLLOUT, "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP, "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR, "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP, "epoll uses same flag values as poll");

const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop) : Poller(loop), epollfd_(epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_SYSFATAL << "EPollPoller::EPollPoller";
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

//EPollPoller的poll主要是调用epoll_wait()来获得活动事件
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) // epoll 轮询 并获得
{
    LOG_TRACE << "fd total count " << channels_.size(); // channels_ 基类中的存放channel的map对象
    int numEvents = epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int savedErrno = errno;
    Timestamp now(std::chrono::system_clock::now());
    if (numEvents > 0)
    {
        LOG_TRACE << numEvents << " events happened";

        fillActiveChannels(numEvents, activeChannels); // 更新已就绪的events事件到activeChannels

        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_TRACE << "nothing happened";
    }
    else
    {
        if (savedErrno != EINTR)
        { // EINTR表示：由于信号中断，没写成功任何数据 处理方式：重启被中断的系统调用
            errno = savedErrno;
            LOG_SYSERR << "EPollPoller::Poll()";
        }
    }
    return now;
}

void EPollPoller::updateChannel(Channel *channel) // 更新这次channel到 map 里 可能是增加, 删除, 或者 修改. 依据channel携带信息的index判断该channel是否已经
// 在这个Epoller对象的epoll中注册过 （）/*  */
{
    Poller::assertInLoopThread();
    const int index = channel->index();
    LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events() << " index = " << index;
    if (index == kNew || index == kDeleted)
    { // 如果是还没加入过的或者被标记为删除的channel则 a new one ,add with EPOLL_CTL_ADD 并且加入map;
        int fd = channel->fd();
        if (index == kNew)
        {
            assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
        }
        else
        { // index = kDeleted时 fd只是在epoll的关注中被移除了 并没有从channels_中remove
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }
        channels_[fd]->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // 更新一个存在的event 通过Epoll_CTL_MOD/DEL
    {
        int fd = channel->fd();
        assert(channels_.find(fd) != channels_.end());
        assert(channels_[fd] == channel);
        assert(index == kAdded);
        if (channel->isNoEvent())
        { // channel的状态是删除
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel *channel) // 从map中移除
{
    Poller::assertInLoopThread();
    int fd = channel->fd();
    LOG_TRACE << "fd = " << fd;
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoEvent());

    int index = channel->index();
    assert(index == kAdded || index == kDeleted);

    size_t n = channels_.erase(fd); // 从map中删除
    assert(n == 1);
    if (index == kAdded) // 从epoll中删除
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    Poller::assertInLoopThread();
    for (int i = 0; i < numEvents; i++)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);

        int fd = channel->fd();
        auto it = channels_.find(fd);
        assert(it != channels_.end());
        assert(it->second == channel);

        channel->set_revents(events_[i].events); // 更新channel的revent事件 
        activeChannels->push_back(channel);
    }
}

void EPollPoller::update(int operation, Channel *channel) // 将channel代表的fd更新到epoll上 并设置好该fd关注的事件（可能是添加 可能是删除 根据operation决定）
{
    epoll_event event;
    bzero(&event, sizeof event);
    event.events = channel->events(); // channel关注的事件
    event.data.ptr = channel;
    int fd = channel->fd();
    LOG_TRACE << "epoll_ctl op = " << operationToString(operation) << " fd = " << fd << " event = {" << channel->eventsToString() << " }";
    if (epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL) // 删除出错 报错
        {
            LOG_SYSERR << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
        }
        else // 添加和更改出错 直接fatal 退出程序
        {
            LOG_SYSFATAL << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
        }
    }
}

const char *EPollPoller::operationToString(int op)
{
    switch (op)
    {
    case EPOLL_CTL_ADD:
        return "ADD";
    case EPOLL_CTL_DEL:
        return "DEL;";
    case EPOLL_CTL_MOD:
        return "MOD";
    default:
        assert(false && "ERROP op");
        return "Unknown Operation";
    }
}
