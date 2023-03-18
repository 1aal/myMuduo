#include "TcpConnection.h"
#include "base/Logger.h"
#include "base/WeakCallback.h"
#include "mynet/EventLoop.h"
#include "mynet/SocketsOps.h"

void defaultConnectionCallback(const TcpConnectionPtr &conn)
{
    LOG_TRACE << conn->localAddress().toIpPort() << " -> "
              << conn->peerAddress().toIpPort() << " is "
              << (conn->connected() ? "UP" : "DOWM");
}

void defaultMessageCallback(const TcpConnectionPtr &conn, Buffer *buf, Timestamp)
{
    buf->retrieveAll(); // 标记读完所有数据 但是实际上还没处理数据。 仅作为默认的回调函数使用
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &name,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(loop),
      name_(name),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) // 64MB

{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_DEBUG << "TcpConnection::dtor[" << name_ << "] at" << this << " fd =" << channel_->fd()
              << " state = " << stateToString();
    assert(state_ == kDisconnected);
}

bool TcpConnection::getTcpInfo(tcp_info *tcpi) const
{ // getsockopt获得tcp的信息
    return socket_->getTcpInfo(tcpi);
}

std::string TcpConnection::getTcpInfoString() const
{
    char buff[1024];
    buff[0] = '\0';
    socket_->getTcpInfoString(buff, sizeof buff);
    return buff;
}

void TcpConnection::send(const void *message, size_t len)
{
    send(std::string(static_cast<const char *>(message), len));
}

//线程安全 可以跨线程调用
void TcpConnection::send(const std::string &message)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        { // 是同一线程
            sendInLoop(message);
        }
        else
        {                                                                                       // 对于有重载的函数的绑定 需要显示的指出被绑定成员函数的类型
            void (TcpConnection::*fp)(const std::string &message) = &TcpConnection::sendInLoop; // fixme
            loop_->runInLoop(std::bind(fp, this, message));//这里拷贝message会带来一定开销
        }
    }
}

void TcpConnection::send(Buffer *buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        }
        else
        {                                                                                                              // bind不能绑定重载了的函数
            void (TcpConnection::*fp /*这是个函数指针别名*/)(const std::string &message) = &TcpConnection::sendInLoop; // 因为sendInLoop是一个重载了的函数 需要用声明新的函数指针明确其调用的是哪一个
            loop_->runInLoop(std::bind(fp, this, buf->retrieveAllAsString()));                                         //
            // std::bind(&TcpConnection::sendInLoop, this, buf->retrieveAllAsString()); 会报错  因为sendInLoop是重载了的函数
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting); //把状态改成正在关闭状态 
        loop_->runInLoop(std::bind(&TcpConnection::shundownInLoop, this));
    }
}

void TcpConnection::forceClose()
{   
    if(state_ == kConnected || state_ == kDisconnecting){
        setState(kDisconnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop,shared_from_this()));
    }
}

void TcpConnection::forceCloseWithDelay(double second)
{
    if(state_ == kConnected || state_ == kDisconnecting){
        setState(kDisconnecting);
        loop_->runAfter(second,makeWeakCallback(shared_from_this(),&TcpConnection::forceClose));
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}

void TcpConnection::startRead()
{
    loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, this));
}

void TcpConnection::stopRead()
{
    loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this));
}

//当有新的客户端连接到来时 从TcpServer中向连接所在的线程注册运行
void TcpConnection::connectEstablished()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);
    channel_->tie(shared_from_this()); //将当前conn对象新的share对象赋值给tie tie是弱引用
    channel_->enableReading();
    connectionCallback_(shared_from_this());
}

//TcpConnection
void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

//channel可读事件触发时 读客户端发来的数据 读到输入缓冲区内
void TcpConnection::handleRead(Timestamp receiveTime)
{
    loop_->assertInLoopThread();
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) //被动关闭连接 读到0说明客户端关闭 调用handleClose
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_SYSERR << "TcpConnection::handleRead";
        handleError();
    }
}

//channel可写事件触发时
void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if (channel_->isWriting())
    {
        // 把输出缓冲区的可读数据写入管道 输出缓冲区数据来源于应用层 输出至管道
        ssize_t n = sockets::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) // 输出缓冲区可读为0即没有数据需要输出到网络时 要写事件取消关注
            {
                channel_->disableWriting(); // 处理完可写数据后要把 写事件取消关注 否则会busy loop 因为是LT模式
                if (writecompleteCallback_) 
                {
                    loop_->queueInLoop(std::bind(writecompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) //这里对应的是shutdown里 如果检查当时conn还在写，那么只将状态改为kDisconnecting,待写完以后shundown
                {
                    shundownInLoop();
                }
            }
        }
        else
        {
            LOG_SYSERR << "TcpConnection::handleWrite";
        }
    }
    else
    {
        LOG_TRACE << "Connection fd = " << channel_->fd()
                  << " is down, no more writing";
    }
}

//Tcp关闭连接的时候为什么需要guradThis
void TcpConnection::handleClose() 
{
    loop_->assertInLoopThread();
    LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
    assert(state_ == kConnected || state_ == kConnecting);
    setState(kDisconnected);
    channel_->disableAll(); // 每个连接有一个连接套接字，在连接断开前，套接字对应的channel还在poller的关注列表中，所以handleClose需要将当前channel对象的所有事件取消关注

    // fixme
    TcpConnectionPtr guradThis(shared_from_this()); //this对象的计数引用+1 直到执行完handleClose
    connectionCallback_(guradThis);//回调用户连接到来的函数 可以不调用
    //通知TcpServer和TcpClient移除持有的TcpConnectionPtr;
    //closeCallback_在建立连接 即Acceptor返回的时候会将TcpServer::removeConnection 注册为TcpConnection对象的closeCallback_
    closeCallback_(guradThis);//调用TcpServer::removeConnection
}

//处理错误
void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->fd());
    LOG_ERROR << "TcpConnection::handleError [" << name_
              << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

void TcpConnection::sendInLoop(const std::string &message)
{
    sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void *data, size_t len)
{
    loop_->assertInLoopThread();
    ssize_t nwrote = 0; // 已发送字节数
    size_t remaining = len;
    bool faultError = false;
    if (state_ == kDisconnected)
    {
        LOG_WARN << "disconnected, givp up writing";
        return;
    }

    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 如果输出缓冲区为空（没有其他待写数据 尝试直接向内核缓冲区写 若能够写完说明不需要输出缓冲区，也不需要再关注写事件）
        nwrote = sockets::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = remaining - nwrote;
            if (remaining == 0 && writecompleteCallback_)
            {
                loop_->runInLoop(bind(writecompleteCallback_, shared_from_this()));
            }
        }
        else
        { 
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            { // EWOULDBLOCK就是EAGIN 发送时接收到表示操作被阻塞了,即内核写缓冲区里已经有数据了 这一批写的数据没有写入 需要重写入
                LOG_SYSERR << "TcpConnection::sendInLoop";
                if (errno == EPIPE || errno == ECONNRESET) // 管道破裂或者连接RST 造成原因可能是客户端掉线/或者对端重启连接，还未建立连接
                {
                    faultError = true;
                }
            }
        }
    }

    assert(remaining <= len);
    if (!faultError && remaining > 0)
    { // 没有发生EPIPE错误或ECONNRESET错误 且待写内容还不为空时说明内核写缓冲区满了 要将剩余的数据写入output缓冲区
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highwatermarkCallback_)//如果应用层的output缓冲区已有内容超过高水位警戒线 并且设置了highwatermarkCallback_
        {       //则调用highwatermarkCallback_处理
            loop_->queueInLoop(bind(highwatermarkCallback_,shared_from_this(),oldLen + remaining));
        }
        outputBuffer_.append(static_cast<const char*>(data)+nwrote,remaining);
        if(!channel_->isWriting()){
            channel_->enableReading(); //如果应用层输出缓冲区有数据 那么需要关注pullout事件
        }
    }
}

void TcpConnection::shundownInLoop()
{
    loop_->assertInLoopThread();
    if (!channel_->isWriting())//若还在关注pullout事件 不能调用shutdownWirte
    {
        socket_->shutdownWrite();//关闭写的这一半
    }
}

void TcpConnection::forceCloseInLoop()//主动关闭连接
{
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kConnecting)
    {
        handleClose();
    }
}

const char *TcpConnection::stateToString() const
{
    switch (state_)
    {
    case kDisconnected:
        return "kDisconnected";
    case kConnecting:
        return "kConnecting";
    case kConnected:
        return "kConnected";
    case kDisconnecting:
        return "kDisconnecting";
    default:
        return "unknown state";
    }
}

void TcpConnection::startReadInLoop()
{
    loop_->assertInLoopThread();
    if (!reading_ || !channel_->isReading())
    { // 与这个connfd绑定的channel在loop中没有检测到读事件
        channel_->enableReading();
        reading_ = true;
    }
}

void TcpConnection::stopReadInLoop()
{
    loop_->assertInLoopThread();
    if (reading_ || channel_->isReading())
    {
        channel_->disableReading();
        reading_ = false;
    }
}
