#include "base/Logger.h"
#include "mynet/EventLoop.h"
#include "mynet/TimerQueue.h"
#include "mynet/Channel.h"
#include <thread>
#include <sstream>
#include <assert.h>
#include <unistd.h>
#include <algorithm> //find()
#include <sys/eventfd.h> //linux下一切皆文件
#include "mynet/Poller.h"
#include "EventLoop.h"

thread_local EventLoop *t_loopInThisThread = nullptr;
thread_local char threadIdBuffer[32] = {'\0'};
const int kPollTimeMS = 10000; //1s

int createEventfd()
{
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_SYSERR << "Failed in eventfd";
        abort();
    }
    return evtfd;
}

char *currentThreadIdToString()
{
    if (strlen(threadIdBuffer) == 0)
    {
        snprintf(threadIdBuffer, 32, "%lld", std::this_thread::get_id());
    }
    return threadIdBuffer;
}

EventLoop::EventLoop() : looping_(false), quit_(false),
                         eventHandling_(false), callingPendingFunctors_(false), iteration_(0),
                         threadId_(std::this_thread::get_id()), poller_(Poller::newDefualtPoller(this)), timerqueue_(new TimerQueue(this)),
                         wakeupFd_(createEventfd()), wakeupChannel_(new Channel(this, wakeupFd_)), currentActiveChannel_(nullptr)
{

    LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
    LOG_DEBUG <<"wakeupFd_ is "<< wakeupFd_;
    if (t_loopInThisThread)
    {
        LOG_FATAL << "Another EventLoop " << t_loopInThisThread << " exists in this thread " << threadId_;
    }
    else
    {
        t_loopInThisThread = this;
    }
    wakeupChannel_->setReadCallback(bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    LOG_DEBUG << "EventLoop " << this << "of thread " << threadId_ << "destructrs is thread " << currentThreadIdToString();
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;
    LOG_TRACE << "EventLoop " << this << " start looping";

    while (!quit_)
    {
        activeChannels_.clear(); // Channel的vector
        //调用具体的poll函数 Epoll就是epoll_wait() Poll就是::poll() ;得到activeChannels_集合
        pollReturnTime_ = poller_->poll(kPollTimeMS, &activeChannels_);
        ++iteration_; //
        if (Logger::logLevel() <= Logger::TRACE)
        {
            printActiveChannels();
        }
        // 各事件回调函数
        eventHandling_ = true;
        for (Channel *channel : activeChannels_)
        {
            currentActiveChannel_ = channel; // 通过channel通知其他线程 进行回调函数
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }
        currentActiveChannel_ = nullptr;
        eventHandling_ = false;

        doPendingFunctors(); // 让IO线程也可以执行一些计算任务 使得利用率变高 而且不会一直检测pendingFunctors_是否为空 否则可能会一直处理计算任务而IO事件得不到检测
    }
    LOG_TRACE << "EventLoop " << this << " stop looping";
    looping_ = false;
}

void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())
    { // quit函数可能再其他线程中被调用 此时需要调用唤醒函数
        wakeup();
    }
}

int64_t EventLoop::iteration() const
{
    return iteration_;
}

void EventLoop::runInLoop(Functor cb) // 在io线程中执行某个回调函数 该函数可以跨线程调用
{
    if (isInLoopThread())
    {
        cb(); // 如果当前IO线程调用了runInLoop 则同步调用 cb
    }
    else
    { // 如果是其他线程调用runInloop  则异步加cb任务添加到队列
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        lock_guard<mutex> lc(mutex_);
        pendingFunctors_.push_back(cb);
    }
    // 调用queueInLoop的线程不是当前IO线程 或者 当前IO线程正在调用pendingfunctor 需要唤醒IO线程 因为IO线程可能处于loop的阻塞状态
    if (!isInLoopThread() || callingPendingFunctors_) // callingPendingFunctors_的状态是true 那么这个线程是IO线程并且正处在doPendingFunctors函数中并调用了queueInLoop
    {
        wakeup(); //
    }
}

size_t EventLoop::queueSize() const
{
    lock_guard<mutex> lc(mutex_);
    return pendingFunctors_.size();
}
EventLoop *EventLoop::getEventLoopOfCurrentThread()
{
    return t_loopInThisThread;
}
void EventLoop::wakeup() // 唤醒 通过fd向loop的fd中发一个信号
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR << "EventLoop::wakeup() wirtes " << n << " bytes instead of 8";
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    if (eventHandling_)
    {
        assert(currentActiveChannel_ == channel || std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end() );
    }
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
    LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
              << " was created in threadId_ = " << threadId_
              << ", current thread id = " << currentThreadIdToString();
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::lock_guard<std::mutex> lc(mutex_);
        functors.swap(pendingFunctors_); // 交换两个容器内容
    }
    for (const Functor &functor : functors)
    {              // 执行回调函数
        functor(); // 这里的functor可能再次调用queueInLoop 需要访问pendingFunctors_ 在这里如果没有解锁的话可能会死锁
    }
    callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
    for (const Channel *channel : activeChannels_)
    {
        LOG_TRACE << "{" << channel->reventsToString() << "}";
    }
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
{
    return timerqueue_->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, cb);
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerqueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId tiemrId)
{
    return timerqueue_->cancel(tiemrId);
}

void EventLoop::assertInLoopThread()
{
    if (!isInLoopThread())
    {
        abortNotInLoopThread();
    }
}

bool EventLoop::isInLoopThread() const
{
    return threadId_ == std::this_thread::get_id();
}

bool EventLoop::eventHandling() const
{
    return eventHandling_;
}
