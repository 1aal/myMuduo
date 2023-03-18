set(base_srcs 
Logger.cpp 
FileUtils.cpp
LogFile.cpp
Timestamp.cpp
)

add_library(muduo_base ${base_srcs})
target_link_libraries(muduo_base pthread rt)

add_subdirectory(tests)