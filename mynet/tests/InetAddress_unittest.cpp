#include "mynet/InetAddress.h"
#include "base/Logger.h"
#include <string>
#include <assert.h>
using namespace std;

int main()
{
    {
        InetAddress addr0(1234);
        cout<<addr0.toIp()<<endl;
        cout<<addr0.toIpPort()<<endl;
        cout<<addr0.port()<<endl;

        // assert(addr0.toIp() == string("0.0.0.0"));
        // assert(addr0.toIpPort() == string("0.0.0.0:1234"));
        // assert(addr0.port() == 1234);

        InetAddress addr1(4321, true);
        assert(addr1.toIp() == string("127.0.0.1"));
        assert(addr1.toIpPort() == string("127.0.0.1:4321"));
        assert(addr1.port() == 4321);

        InetAddress addr2("1.2.3.4", 8888);
        assert(addr2.toIp() == string("1.2.3.4"));
        assert(addr2.toIpPort() == string("1.2.3.4:8888"));
        assert(addr2.port() == 8888);

        InetAddress addr3("255.254.253.252", 65535);
        assert(addr3.toIp() == string("255.254.253.252"));
        assert(addr3.toIpPort() == string("255.254.253.252:65535"));
        assert(addr3.port() == 65535);
    }

    InetAddress addr0(1234, false, true);
    assert(addr0.toIp() == string("::"));
    assert(addr0.toIpPort() == string("[::]:1234"));
    assert(addr0.port() == 1234);

    InetAddress addr1(1234, true, true);
    assert(addr1.toIp() == string("::1"));
    assert(addr1.toIpPort() == string("[::1]:1234"));
    assert(addr1.port() == 1234);

    InetAddress addr2("2001:db8::1", 8888, true);
    assert(addr2.toIp() == string("2001:db8::1"));
    assert(addr2.toIpPort() == string("[2001:db8::1]:8888"));
    assert(addr2.port() == 8888);

    InetAddress addr3("fe80::1234:abcd:1", 8888);
    assert(addr3.toIp() == string("fe80::1234:abcd:1"));
    assert(addr3.toIpPort() == string("[fe80::1234:abcd:1]:8888"));
    assert(addr3.port() == 8888);

    InetAddress addr(80);
    if (InetAddress::resolve("baidu.com", &addr)) //调用gethostbyname_r获取网站名hostname对应的ip地址
    {
        LOG_INFO << "google.com resolved to " << addr.toIpPort();
    }
    else
    {
        LOG_ERROR << "Unable to resolve google.com";
    }
}