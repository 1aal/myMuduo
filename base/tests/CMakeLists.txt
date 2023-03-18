add_executable(LogerT Logger_test.cpp)
target_link_libraries(LogerT muduo_base)

add_executable(LogFile LogFile_test.cpp)
target_link_libraries(LogFile muduo_base)
