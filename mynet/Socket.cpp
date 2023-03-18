#include "Socket.h"
#include "base/Logger.h"
#include "mynet/SocketsOps.h"
#include "mynet/InetAddress.h"
#include <netinet/in.h>
#include <netinet/tcp.h> //tcp_info
#include <stdio.h>       //snprinf

Socket::~Socket()
{
    sockets::close(sockfd_); // RAII 运行到这里的时候表示主线程中的TcpServer正在析构 导致了TcpServer对象持有的Acceptor正在析构 导致了Acceptor持有的socket对象正在析构
}

bool Socket::getTcpInfo(tcp_info *tcpi) const
{
    socklen_t len = sizeof(*tcpi);
    bzero(tcpi, len);
    return getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::getTcpInfoString(char *buf, int len) const
{
    tcp_info tcpi;
    bool ok = getTcpInfo(&tcpi);
    if (ok)
    {
        snprintf(buf, len, "unrecovered=%u "
                           "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
                           "lost=%u retrans=%u rtt=%u rttvar=%u "
                           "sshthresh=%u cwnd=%u total_retrans=%u",
                 tcpi.tcpi_retransmits, // Number of unrecovered [RTO] timeouts
                 tcpi.tcpi_rto,         // Retransmit timeout in usec
                 tcpi.tcpi_ato,         // Predicted tick of soft clock in usec
                 tcpi.tcpi_snd_mss,
                 tcpi.tcpi_rcv_mss,
                 tcpi.tcpi_lost,    // Lost packets
                 tcpi.tcpi_retrans, // Retransmitted packets out
                 tcpi.tcpi_rtt,     // Smoothed round trip time in usec
                 tcpi.tcpi_rttvar,  // Medium deviation
                 tcpi.tcpi_snd_ssthresh,
                 tcpi.tcpi_snd_cwnd,
                 tcpi.tcpi_total_retrans); // Total retransmits for entire connection)
    }
    return ok;
}

//将本地需要监听的地址绑定到sock fd上
void Socket::bindAddress(const InetAddress &localaddr)
{
    sockets::bindOrDie(sockfd_, localaddr.getSockAddr());
}

//监听
void Socket::listen()
{
    sockets::listenOrDie(sockfd_);
}

int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in6 addr;
    bzero(&addr, sizeof addr);
    int connfd = sockets::accept(sockfd_, &addr);
    if (connfd >= 0)
    {
        peeraddr->setSockAddrInet6(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    sockets::shutdownWrite(sockfd_);
}

// Nagle算法一定程度上避免网络拥塞
// TCP_NODELAY选项可以禁用Nagle算法
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    int optlen = static_cast<socklen_t>(optval);
    setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, optlen);
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    int optlen = static_cast<socklen_t>(optval);
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    int optlen = static_cast<socklen_t>(optval);
    int ret = setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, optlen);
    if (ret < 0 && on)
    {
        LOG_SYSERR << "SO_REUSEPORT failed.";
    }
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    int optlen = static_cast<socklen_t>(optval);
    setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);
}
