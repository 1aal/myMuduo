#include "Acceptor.h"
#include "mynet/SocketsOps.h"
#include "mynet/InetAddress.h"
#include "mynet/EventLoop.h"
#include "base/Logger.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport) : loop_(loop),
                                                                                     acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())), // sock fd在Acceptor类中创建 这个fd是监听有没有新连接到来的fd
                                                                                     acceptChannel_(loop_, acceptSocket_.fd()),                           // 把这个fd封装到一个channel里面
                                                                                     listening_(false),
                                                                                     idleFd_(open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    assert(idleFd_ >= 0);
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);                                  // 对这个sock fd完成了bind操作
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this)); // 对channel注册了这个sockfd有可读事件发生（即有新连接到来）时的回调函数
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    close(idleFd_);
}

// 接受新客户端连接，并且以负载均衡的选择方式选择一个sub EventLoop，并把这个新连接分发到这个subEventLoop上
void Acceptor::handleRead()
{
    loop_->assertInLoopThread();
    InetAddress peerAddr;
    int confd = acceptSocket_.accept(&peerAddr);
    if (confd >= 0)
    {
        if (newconnectionCallback_)
        {
            newconnectionCallback_(confd, peerAddr);
        }
        else
        {
            sockets::close(confd);
        }
    }
    else
    {   //系统以及没有足够的空间为新来的连接分配conn_fd了 那么用idleFd接收这个连接并马上关闭 
        LOG_SYSERR << "in Acceptor::handleRead";
        if (errno == EMFILE)
        {
            close(idleFd_);
            idleFd_ = accept(acceptSocket_.fd(), NULL, NULL);
            close(idleFd_);
            idleFd_ = open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}

void Acceptor::listen()
{
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}
