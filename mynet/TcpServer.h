#pragma once

#include <atomic>
#include <map>
#include "mynet/TcpConnection.h"
#include "base/Noncopyable.h"

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

class TcpServer : noncopyable
{

public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    enum Option
    {
        kNoReusePort,
        kReusePort
    };
    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option op = kNoReusePort);

    ~TcpServer();
    //设置处理输入的线程数
    //新的连接永远在loop线程中accept
    //@param numThreads
    // - 0 意味着所有的I/O都在loop线程中进行
    // - 1 表示着创建另一个新线程去处理I/O操作
    // - N 表示创建一个线程池大小为N,新连接到来分派到哪个线程池依据轮询算法
    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    //必须在调用start()函数之后调用
    std::shared_ptr<EventLoopThreadPool> threadPool()
    {
        return threadPool_;
    }

    /// Starts the server if it's not listening.
    ///
    /// It's harmless to call it multiple times.
    /// Thread safe.
    void start();

    //暴露给用户的接口 设置新连接到来后的回调函数 非线程安全
    void setConnectionCallback(const ConnectionCallback& cb){
        connectionCallback_ = cb;
    }
    //暴露给用户的接口 设置消息回调函数（有数据已经从内核接收缓冲区读到应用层输入缓冲区后的回调） 非线程安全
    void setMessageCallback(const MessageCallback& cb){
        messageCallback_ = cb;
    }
    //暴露给用户的接口 设置写完成回调函数
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){
        writeCompleteCallback_ = cb;
    }
    


private:
    // 非线程安全的 但是in loop
    void newConnection(int sockfd, const InetAddress &peerAddr);
    // 线程安全
    void removeConnection(const TcpConnectionPtr &conn);
    // 非线程安全 但是in loop
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::map<string, TcpConnectionPtr>;

    EventLoop *loop_; // the acceptor loop 也就是main loop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;
    std::atomic<int32_t> started_ = 0;
    int nextConnId_;
    ConnectionMap connections_;
};

