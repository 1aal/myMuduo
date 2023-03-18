#pragma once 
#include<arpa/inet.h>
//全局函数
//封装了socket套接字生命流程中的相关的系统调用
namespace sockets
{
    int createNonblockingOrDie(sa_family_t family); //创建一个非阻塞的套接字fd 如果失败终止程序
    
    int connect(int sockfd, const sockaddr* addr);
    void bindOrDie(int sockfd, const sockaddr* addr);
    void listenOrDie(int sockfd);
    int accept(int sockfd,sockaddr_in6 *addr);
    size_t read(int sockfd,void *buf,size_t count);
    size_t readv(int sockfd,const iovec *iov, int iovcnt);
    size_t write(int sockfd, const void* buf,  size_t count);
    void close(int sockfd);
    void shutdownWrite(int sockfd);
    void toIpPort(char* buf,size_t size,const sockaddr* addr);
    void toIp(char* buf,size_t size, const sockaddr* addr);
    void fromIpPort(const char* ip,uint16_t port, sockaddr_in* addr);
    void fromIpPort(const char* ip,uint16_t port, sockaddr_in6* addr);

    int getSocketError(int sockfd);

    const sockaddr* sockaddr_cast(const sockaddr_in* addr);
    const sockaddr* sockaddr_cast(const sockaddr_in6* addr);
    sockaddr* sockaddr_cast(sockaddr_in6* addr);

    const sockaddr_in* sockaddr_in_cast(const sockaddr* addr);
    const sockaddr_in6* sockaddr_in6_cast(const sockaddr* addr);

    sockaddr_in6 getLocalAddr(int sockfd);
    sockaddr_in6 getPeerAddr(int sockfd);

    bool isSelfConnect(int sockfd);

} // namespace sockets
