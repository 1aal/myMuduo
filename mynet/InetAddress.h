//InetAddress：  网际地址socket_in的封装 将hostname的地址转化为他们的网络ip地址 调用了 sockets命名空间的函数

#pragma once  

#include <netinet/in.h>
#include <string>

namespace sockets{
    const sockaddr* sockaddr_cast(const sockaddr_in6* addr);
}


class InetAddress
{
private:
    union 
    {
        sockaddr_in addr_;
        sockaddr_in6 addr6_;
    };
    
public:
    explicit InetAddress(uint16_t port = 0,bool loopbackOnly = false, bool ipv6 = false);
    InetAddress(std::string ip,uint16_t port, bool ipv6 = false);
    explicit InetAddress(const sockaddr_in& addr):addr_(addr){}
    explicit InetAddress(const sockaddr_in6& addr6):addr6_(addr6){}

    sa_family_t family() const{return addr_.sin_family;}
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t port() const;

    const sockaddr* getSockAddr() const{ return sockets::sockaddr_cast(&addr6_);}//返回Sock通用地址格式
    void setSockAddrInet6(const sockaddr_in6& addr6){ addr6_ = addr6; }
    uint32_t ipv4NetEndian() const;
    uint16_t portNetEndian() const { return addr_.sin_port ;} //返回网络端的port值

    //用gethostbyname_r获取网站名hostname对应的ip地址
    static bool resolve(std::string hostname ,InetAddress* result);//
    void setScopeId(uint32_t scope_id);

};

