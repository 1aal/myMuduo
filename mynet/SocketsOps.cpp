#include "SocketsOps.h"
#include "base/Logger.h"
#include "mynet/Endian.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>   //snprintf
#include <sys/uio.h> //readv
#include <unistd.h>
#include <assert.h>
#include <string.h>

using SA = sockaddr;

void setNonBlockAndCloseOnExec(int sockfd) // 设置文件描述符为NonBlockAndCloseOnExec
{

    /* fcntl 函数改变一个已打开的文件的属性,可以重新设置读、写、追加、非阻塞等标志(这些标志称为File StatusFlag),而不必重新open 文件。
    • 复制一个现存的描述符(cmd=F_DUPFD)    。
    • 获得/设置文件描述符标记(cmd=F_GETFD或F_SETFD)   。
    • 获得/设置文件状态标志(cmd=F_GETFL或F_SETFL) 。
    • 获得/设置异步I/O有权(cmd=F_GETOWN或F_SETOWN) 。
    • 获得/设置记录锁(cmd=F_GETLK,F_SETLK或F_SETLKW)。 */
    // Non Block
    int flags = fcntl(sockfd, F_GETFL, 0); // 读取文件状态标志。
    flags |= O_NONBLOCK;
    int ret = fcntl(sockfd, F_SETFL, flags);

    // CloseOnExec
    flags = fcntl(sockfd, F_GETFD, 0); // 读取文件描述词标志
    flags |= O_CLOEXEC;
    ret = fcntl(sockfd, F_SETFD, flags);
}

const sockaddr *sockets::sockaddr_cast(const sockaddr_in *addr) // 将ipv4地址转化为socket通用地址
{
    return static_cast<const sockaddr *>(reinterpret_cast<const void *>(addr)); // static_cast无法进行无关类型指针间的转换
}

const sockaddr *sockets::sockaddr_cast(const sockaddr_in6 *addr) // 将ipv6地址转化为socket通用地址
{
    return static_cast<const sockaddr *>(reinterpret_cast<const void *>(addr));
}

sockaddr *sockets::sockaddr_cast(sockaddr_in6 *addr) // 将ipv6地址转化为sock通用地址
{
    return static_cast<sockaddr *>(reinterpret_cast<void *>(addr));
}

const sockaddr_in *sockets::sockaddr_in_cast(const sockaddr *addr)
{
    return static_cast<const sockaddr_in *>(reinterpret_cast<const void *>(addr));
}

const sockaddr_in6 *sockets::sockaddr_in6_cast(const sockaddr *addr)
{
    return static_cast<const sockaddr_in6 *>(reinterpret_cast<const void *>(addr));
}

int sockets::createNonblockingOrDie(sa_family_t family) // 创建Nonblocking SOCK_CLOEXEC socket文件描述符
{
    int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_SYSFATAL << "sockets::createNonblockingOrDie";
    }
    // setNonBlockAndCloseOnExec(sockfd);
    return sockfd;
}

void sockets::bindOrDie(int sockfd, const sockaddr *addr)
{
    int ret = ::bind(sockfd, addr, static_cast<socklen_t>(sizeof(sockaddr_in6)));
    if (ret < 0)
    {
        LOG_SYSFATAL << "socket::bindOrDie";
    }
}

void sockets::listenOrDie(int sockfd)
{
    int ret = ::listen(sockfd, SOMAXCONN); // SOMAXCONN: Maximum queue length specifiable by listen.
    if (ret < 0)
    {
        LOG_SYSFATAL << "sockets::listenOrDie";
    }
}

int sockets::accept(int sockfd, sockaddr_in6 *addr)
{
    socklen_t addrlen = static_cast<socklen_t>(sizeof *addr);
    int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen); // accept4()函数共有4个参数，相比accept()多了一个flags的参数，用户可以通过此参数直接设置套接字的一些属性，如SOCK_NONBLOCK或者是SOCK_CLOEXEC。当accept4的flags为0的时候，accept4和accept没有区别。
    setNonBlockAndCloseOnExec(connfd);
    if (connfd < 0)
    {
        int savedErrno = errno;
        LOG_SYSERR << "Socket::accept";
        switch (savedErrno)
        {
        case EAGAIN:
        case ECONNABORTED:
        case EINTR:
        case EPROTO: // ???
        case EPERM:
        case EMFILE: // per-process lmit of open file desctiptor ???
            // expected errors
            errno = savedErrno;
            break;
        case EBADF:
        case EFAULT:
        case EINVAL:
        case ENFILE:
        case ENOBUFS:
        case ENOMEM:
        case ENOTSOCK:
        case EOPNOTSUPP:
            // unexpected errors
            LOG_FATAL << "unexpected error of ::accept " << savedErrno;
            break;
        default:
            LOG_FATAL << "unknown error of ::accept " << savedErrno;
            break;
        }
    }
    return connfd;
}

int sockets::connect(int sockfd, const sockaddr *addr)
{
    return connect(sockfd, addr, static_cast<socklen_t>(sizeof(sockaddr_in6)));
}

// read 会从 fd 当前的 offset 处开始读，读取完 nbyte 后，
// 该 fd 的 offset 会增加 nbyte（假设可以读取到 nbyte），
// 下一次 read 则从新的 offset 处开始读。
size_t sockets::read(int sockfd, void *buf, size_t count)
{

    return ::read(sockfd, buf, count);
}

// readv 则是从 fd 中读取数据到多个 buf，buf 数为 iovcnt，每个 buf 有自己的长度（可以一样）
size_t sockets::readv(int sockfd, const iovec *iov, int iovcnt)
{
    // readv 则是从 fd 中读取数据到多个 buf，buf 数为 iovcnt，每个 buf 有自己的长度（可以一样），
    // 一个 buf 写满（写指读出数据并保存），才接着写下一个 buf，依次类推
    return ::readv(sockfd, iov, iovcnt);

    // 因为使用read()将数据读到不连续的内存、
    // 使用write()将不连续的内存发送出去，要经过多次的调用read、write
    //  writev以顺序iov[0]、iov[1]至iov[iovcnt-1]从各缓冲区中聚集输出数据到fd
    //  readv则将从fd读入的数据按同样的顺序散布到各缓冲区中，readv总是先填满一个缓冲区，
    //  然后再填下一个
}

// 写入count字节的BUF数据到 FD. Return the number written, or -1
size_t sockets::write(int sockfd, const void *buf, size_t count)
{
    return ::write(sockfd, buf, count);
}

void sockets::close(int sockfd)
{
    if (::close(sockfd) < 0)
    {
        LOG_SYSERR << "sockets::close";
    }
}

void sockets::shutdownWrite(int sockfd) // 关闭套接字并关闭写通道
{                                       /*close与shutdown的区别主要表现在：
                                        close函数会关闭套接字ID，如果有其他的进程共享着这个套接字，那么它仍然是打开的，这个连接仍然可以用来读和写，并且有时候这是非常重要的 ，特别是对于多进程并发服务器来说。
                                      
                                        而shutdown会切断进程共享的套接字的所有连接，不管这个套接字的引用计数是否为零，那些试图读得进程将会接收到EOF标识，那些试图写的进程将会检测到SIGPIPE信号，同时可利用shutdown的第二个参数选择断连的方式。
                                    */
    if (::shutdown(sockfd, SHUT_WR) < 0)
    {
        LOG_SYSERR << "sockets::shutdownWirte";
    }
}

void sockets::toIpPort(char *buf, size_t size, const sockaddr *addr) // 将网络传输的信息转化为点分十进制ip地址和端口
{
    if (addr->sa_family == AF_INET6)
    {
        buf[0] = '[';
        toIp(buf + 1, size - 1, addr);
        size_t end = strlen(buf);
        const sockaddr_in6 *addr6 = sockaddr_in6_cast(addr);
        uint16_t port = sockets::networkToHost16(addr6->sin6_port);
        assert(size > end);
        snprintf(buf + end, size - end, "]:%u", port);
        return;
    }
    toIp(buf, size, addr);
    size_t end = strlen(buf);
    const sockaddr_in *addr4 = sockaddr_in_cast(addr);
    uint16_t port = networkToHost16(addr4->sin_port);
    assert(size > end);
    snprintf(buf + end, size - end, ":%u", port);
}

void sockets::toIp(char *buf, size_t size, const sockaddr *addr) // 将网络传输的数组转化为点分十进制并存放在buf中
{
    if (addr->sa_family == AF_INET)
    {
        assert(size >= INET_ADDRSTRLEN); // 断言提供的缓冲区大小一定大于网际地址的长度
        const sockaddr_in *addr4 = sockaddr_in_cast(addr);
        inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(size));
    }
    else if (addr->sa_family == AF_INET6)
    {
        assert(size >= INET6_ADDRSTRLEN);
        const sockaddr_in6 *addr6 = sockaddr_in6_cast(addr);
        inet_ntop(AF_INET6, &addr6->sin6_addr, buf, static_cast<socklen_t>(size));
    }
}

// 将点分十进制ipv4地址转化为用于网络传输的数值
void sockets::fromIpPort(const char *ip, uint16_t port, sockaddr_in *addr) 
{
    addr->sin_family = AF_INET;
    addr->sin_port = hostToNetwork16(port);
    if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
    {
        LOG_SYSERR << "sockets::fromIpPort";
    }
}
// 将点分十进制ipv6地址转化为用于网络传输的数值
void sockets::fromIpPort(const char *ip, uint16_t port, sockaddr_in6 *addr) 
{
    addr->sin6_family = AF_INET6;
    addr->sin6_port = hostToNetwork16(port);
    if (inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0)
    {
        LOG_SYSERR << "sockets::fromIpPort";
    }
}

//通过getsockopt获取套接字错误选项
int sockets::getSocketError(int sockfd) // 
{
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    { // 获取一个套接字的选项
        return errno;
    }
    return optval;
}
//通过::getsockname调用获取本机地址信息
sockaddr_in6 sockets::getLocalAddr(int sockfd) // 获取sockfd的本地ip地址
{
    sockaddr_in6 localaddr;
    bzero(&localaddr, sizeof localaddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
    if (getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0)
    { // 获取sockfd的本地ip地址
        LOG_SYSERR << "socket::getLocalAddr()";
    }
    return localaddr;
}

sockaddr_in6 sockets::getPeerAddr(int sockfd) // 获取连接到sockfd的客户端ip地址
{
    struct sockaddr_in6 peeraddr;
    bzero(&peeraddr, sizeof peeraddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
    if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0)
    {
        LOG_SYSERR << "sockets::getPeerAddr()";
    }
    return peeraddr;
}

bool sockets::isSelfConnect(int sockfd) // 判断是否是自连接
{
    sockaddr_in6 localaddr = getLocalAddr(sockfd);
    sockaddr_in6 peeraddr = getPeerAddr(sockfd);
    if (localaddr.sin6_family == AF_INET)
    {
        const sockaddr_in *laddr4 = reinterpret_cast<sockaddr_in *>(&localaddr);
        const sockaddr_in *paddr4 = reinterpret_cast<sockaddr_in *>(&peeraddr);
        return laddr4->sin_port == paddr4->sin_port && laddr4->sin_addr.s_addr == paddr4->sin_addr.s_addr;
    }
    else if (localaddr.sin6_family == AF_INET6)
    {
        return localaddr.sin6_port == peeraddr.sin6_port && memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof localaddr.sin6_addr) == 0;
    }
    return false;
}
