set(net_SRCS
    Channel.cpp
    EventLoop.cpp
    TimerQueue.cpp
    Timer.cpp
    poller/DefaultPoller.cpp
    poller/PollPoller.cpp
    poller/EPollPoller.cpp  
    Poller.cpp
    EventLoopThread.cpp
    SocketsOps.cpp
    Socket.cpp
    InetAddress.cpp
    Acceptor.cpp
    EventLoopThreadPool.cpp
    Buffer.cpp
    TcpConnection.cpp
    TcpServer.cpp
)

add_library(muduonet ${net_SRCS}) #生成静态库
target_link_libraries(muduonet muduo_base)
enable_testing()
add_subdirectory(tests)