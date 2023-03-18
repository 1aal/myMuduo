cmake_minimum_required(VERSION 3.21.5)

set(CMAKE_C_COMPILE /usr/bin/gcc)
set(CMAKE_CXX_COMPILE /usr/bin/g++)


project(myMuduo)

# add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
include(CTest)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug) #调试模式

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

include_directories(${PROJECT_SOURCE_DIR})

add_subdirectory(base)
enable_testing()
add_subdirectory(mynet)


