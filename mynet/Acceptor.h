#pragma once
// 持有的是sockfd 接收并处理sockfd的连接
#include "base/Noncopyable.h"
#include "mynet/Socket.h"
#include "mynet/Channel.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

private:
    void handleRead();
    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newconnectionCallback_;
    bool listening_;
    int idleFd_;

public:
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();
    //有新的客户端请求连接到来的时候调用的回调函数 Acceptor类中只处理新连接到来的回调函数
    void setNewConnectionCallback_(const NewConnectionCallback &cb)
    {
        newconnectionCallback_ = cb;
    };

    void listen();

    bool listening() const { return listening_; }
};
