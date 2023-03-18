#include "TcpServer.h"
#include "mynet/EventLoop.h"
#include "mynet/EventLoopThreadPool.h"
#include "mynet/SocketsOps.h"
#include "base/Logger.h"
#include "mynet/Acceptor.h"

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option op) : loop_(loop), ipPort_(listenAddr.toIpPort()), name_(nameArg), acceptor_(new Acceptor(loop, listenAddr, op == kReusePort)), threadPool_(new EventLoopThreadPool(loop, name_)),
                                                                                                              connectionCallback_(defaultConnectionCallback), messageCallback_(defaultMessageCallback), nextConnId_(1)
{
    acceptor_->setNewConnectionCallback_(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer[" << name_ << "] destructing";
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second); // 生成conn对象 引用计数+1
        item.second.reset();                // 释放原conn对象 引用计数减一
        conn->getLoop()->runInLoop(
            bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    if (started_ == 0)
    { // 确保只启动一次
        threadPool_->start(threadInitCallback_);
        assert(!acceptor_->listening());
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 新客户端连接到来时 需要接收一个sockfd是客户端与服务器通信的fd 以及对端地址
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    loop_->assertInLoopThread();
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_; //
    std::string connName = name_ + buf;
    LOG_INFO << "TcpServer::newConnection [" << name_
             << "] - new connection [" << connName
             << "] from " << peerAddr.toIpPort();
    InetAddress localAddr(sockets::getLocalAddr(sockfd));
    //TcpConnectionPtr是智能指针 离开作用域会销毁
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, placeholders::_1));
    
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

// 非线程安全
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop[" << name_ << "] - connection " << conn->name();
    size_t n = connections_.erase(conn->name());
    assert(n == 1);
    EventLoop* ioloop = conn->getLoop();
    ioloop->queueInLoop(bind(&TcpConnection::connectDestroyed,conn));
}
