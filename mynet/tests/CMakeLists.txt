add_executable(channel_test Channel_test.cpp)
target_link_libraries(channel_test muduonet)

add_executable(reactortest1 reactor_test1.cpp)
target_link_libraries(reactortest1 muduonet)

add_executable(timerqueue_test1 timerqueue_test1.cpp)
target_link_libraries(timerqueue_test1 muduonet)

add_executable(IdbuffTest threadIdbuffertest.cpp)
target_link_libraries(IdbuffTest muduonet)

add_executable(EventLoopThread_test EventLoopThread_test.cpp)
target_link_libraries(EventLoopThread_test muduonet)

add_executable(InetAddress_unittest InetAddress_unittest.cpp)
target_link_libraries(InetAddress_unittest muduonet)

add_executable(EventLoopThreadPool_unittest EventLoopThreadPool_unittest.cpp)
target_link_libraries(EventLoopThreadPool_unittest muduonet)

add_executable(EchoServer_test EchoServer_test.cpp)
target_link_libraries(EchoServer_test muduonet)
# EchoServer_test.cpp
# add_executable(Buffer_test Buffer_test.cpp)
# target_link_libraries(Buffer_test muduonet)

# add_test( #告诉Cmake 有这个测试
#     NAME BufferTEST
#     COMMAND Buffer_test #COMMAND是执行的目标
# )