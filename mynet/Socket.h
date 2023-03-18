
#pragma once
// 用RAII的方法封装 socket fd。 注意 是先被Accept监听到了 打开了一个socket,得到了一个socket fd,再将这个fd传给Socket类进行包装操作
// 而不是在这个socket类中打开的sockfd , 这个类也能accept得到新的fd,但这个fd是客户端与服务器通讯时用的fd
#include "base/Noncopyable.h"
struct tcp_info; // struct tcp_info is in <netinet/tcp.h>
class InetAddress;

class Socket : noncopyable // 文件描述符是系统资源 不允许拷贝
{
private:
    const int sockfd_;

public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    Socket(Socket &&) = default;
    ~Socket();

    int fd() const { return sockfd_; }

    bool getTcpInfo(tcp_info *) const;
    //返回的是这个TCP连接的属性信息 比如客户端发送窗口大小 服务端发送窗口大小等
    bool getTcpInfoString(char *buf, int len) const;

    // 调用bind绑定服务器IP端口
    void bindAddress(const InetAddress &localaddr);

    // 调用listen套接字
    void listen();

    // 调用accept套接字接收客户新连接 并把客户信息存储在peeraddr 并返回客户端与服务器的conn_fd
    int accept(InetAddress *peeraddr);

    // 调用shutdown关闭服务器写通道
    void shutdownWrite();
    /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
    void setTcpNoDelay(bool on);
    /// Enable/disable SO_REUSEADDR
    void setReuseAddr(bool on);
    /// Enable/disable SO_REUSEPORT
    void setReusePort(bool on);
    /// Enable/disable SO_KEEPALIVE
    void setKeepAlive(bool on);
};
