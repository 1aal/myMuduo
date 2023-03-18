#pragma once

#include "base/Noncopyable.h"
#include "mynet/Callbacks.h"
#include "mynet/Buffer.h"
#include "mynet/InetAddress.h"
#include "mynet/Channel.h"
#include "mynet/Socket.h"

#include <any>
#include <memory>
#include <netinet/tcp.h>

struct tcp_info;

// class Channel;
class EventLoop;
// class Socket;

class TcpConnection : noncopyable, public enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop, const std::string &name, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }

    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }
    bool disconnect() const { return state_ == kDisconnected; }

    bool getTcpInfo(tcp_info *) const;
    std::string getTcpInfoString() const;
    //线程安全 可以跨线程调用
    void send(const void *message, size_t len);
    void send(const std::string &message);
    void send(Buffer *message);
    void shutdown();
    void forceClose();

    void forceCloseWithDelay(double second);
    void setTcpNoDelay(bool on);

    void startRead();
    void stopRead();
    bool isReading() const { return reading_; }

    void setContext(const std::any &context) { context_ = context; };
    const std::any &getContext() const { return context_; };
    std::any *getMutableContext() { return &context_; }
    //设置连接到来回调函数 在TcpServer中新建一个Conn对象后 调用
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }
    //设置消息到来回调函数 在TcpServer中新建一个Conn对象后 调用
    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }
    //设置写完成回调函数 在TcpServer中新建一个Conn对象后 调用
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writecompleteCallback_ = cb;
    }
    //设置高水位回调函数 在TcpServer中新建一个Conn对象后 调用
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {
        highwatermarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }
    //设置连接关闭回调函数  
    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }

    Buffer *ipputBuffer()
    {
        return &inputBuffer_;
    }

    Buffer *outputBuffer()
    {
        return &outputBuffer_;
    }

    void connectEstablished(); // 只能调用一次

    void connectDestroyed(); // 只能调用一次

private:
    enum StateE
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const std::string &message);
    void sendInLoop(const void *message, size_t len);
    void shundownInLoop();

    void forceCloseInLoop();
    void setState(StateE s) { state_ = s; };
    const char *stateToString() const;
    void startReadInLoop();
    void stopReadInLoop();

    EventLoop *loop_;
    const std::string name_;
    StateE state_;
    bool reading_;
    // 使用智能指针作为成员变量，被持有对象的类，是不可以进行前置声明的
    std::unique_ptr<Socket> socket_;   // 封装了一个文件描述符以及对应的bind listen accep shutdownWritet等操作
    std::unique_ptr<Channel> channel_; // 连接一个EventLoop和一个打开的文件描述符的桥梁 能够注册/删除文件描述符到loop 和设置相关的回调函数
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    //大流量服务需要设置writecompleteCallback_ 因为大流量会不断生成数据 然后发送conn->send(),如果对方接收不及时 受滑动窗口控制，内核发送缓冲区不足
    //这时候用户会将数据添加到应用层发送缓冲区output buffer;可能会撑爆output buffer
    //解决方法是调整发送频率 关注writecompleteCallback_ 所有的数据发送完，writecompleteCallback_回调，然后再继续发送
    WriteCompleteCallback writecompleteCallback_; //可以理解为低水位标回调函数
    HighWaterMarkCallback highwatermarkCallback_;//高水位标回调函数
    CloseCallback closeCallback_;
    size_t highWaterMark_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    std::any context_;
};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;