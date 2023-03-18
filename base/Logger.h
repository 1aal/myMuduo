#pragma once
//前端主要实现异步日志中的日志功能，为用户提供将日志内容转换为字符串，封装为一条完整的log消息存放到RAM中；
//前端部分包括 Logger.h LogStream.h 具体类包括FixedBuffer模板类,LogStream类,SourceFile类,Impl类和Logger类
#include "base/LogStream.h"
#include <base/Timestamp.h>

#include <string>
#include <fstream>
#include <iostream>
#include <string.h>
#include <chrono>
using namespace std;

class SourceFile // 内部类 对构造Logger对象的代码所在文件名进行包装 只记录基本文件名不含路径 节约日志长度
{
public:
    const char *data_;
    int size_;

    template <int N>
    SourceFile(const char (&a)[N]) : data_(a), size_(N - 1) // const char(&a)[N] 是一个const char大小为N的数组的引用
    {
        const char *slash = strrchr(a, '/'); // 查找后缀
        if (slash)
        {
            data_ = slash + 1;
            size_ -= static_cast<int>(data_ - a);
        }
    }
    explicit SourceFile(const char *filename) : data_(filename)
    {
        const char *slash = strrchr(filename, '/'); // 查找后缀
        if (slash)
        {
            data_ = slash + 1;
        }
        size_ = static_cast<int>(strlen(data_));
    }
};

class Logger // 主要为用户（前端线程）提供使用日志库的接口，是一个pointer to impl的实现（即GoF 桥接模式），详细由内部类Impl实现。
{
public:
    enum LogLevel
    {
        TRACE = 0,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUMS_LOG_LEVEL,
    };

private:
    class Impl // Logger::Impl是Logger的内部类，负责Logger主要实现，提供组装一条完整log消息的功能。
    {
    public:
        typedef Logger::LogLevel LogLevel;
        Impl(LogLevel level, int old_errno, const SourceFile &file, int line);
        void formatTime();
        void finish();

        // Timestamp time_;
        // typedef chrono::system_clock::time_point Timestamp;
        Timestamp time_; // system_clock to_time_t()转化到time_t
        LogStream stream_;
        LogLevel level_;
        int line_;
        SourceFile basename_;
    };

    Impl impl_;

public:
    Logger() = delete;
    Logger(SourceFile file, int line);                                   // 带文件 行数信息
    Logger(SourceFile file, int line, LogLevel level);                   // 带文件 行数信息 Log等级
    Logger(SourceFile file, int line, LogLevel level, const char *func); // 带文件 行数信息 Log等级 函数名信息
    Logger(SourceFile file, int line, bool toAbort);                     // 是否是严重错误 退出程序
    ~Logger();

    static LogLevel logLevel();              // 返回level类型
    static void setLogLevel(LogLevel level); // 设置level类型

    typedef void (*OutputFunc)(const char *msg, int len); // 声明输出流函数
    typedef void (*FlushFunc)();                          // 声明落盘刷新函数

    static void setOutput(OutputFunc); // 设置输出流
    static void setFlush(FlushFunc);   // 设置 落盘刷新函数

    LogStream &stream();
};

// extern Logger::LogLevel g_logLevel; //记录日志等级的全局变量 在.cpp里声明定义 这里extern表示使用那个已经声明定义了的全局

const char *strerror_tl(int savedErrno); // 通过错误码获得错误消息

Logger::LogLevel initLogLevel(); // 初始化日志等级

// Logger::Logger(/* args */)
// {
// }

// Logger::~Logger()
// {
// }
// 当前等级 <= TRACE 等级  输出日志
// 当前等级 > TRACE 等级  不输出日志
#define LOG_TRACE                                          \
    if (Logger::logLevel() <= Logger::TRACE) \
    Logger(__FILE__, __LINE__, Logger::TRACE, __func__).stream()
#define LOG_DEBUG                                          \
    if ( Logger::logLevel() <=  Logger::DEBUG) \
     Logger(__FILE__, __LINE__,  Logger::DEBUG, __func__).stream()
#define LOG_INFO                                          \
    if ( Logger::logLevel() <=  Logger::INFO) \
     Logger(__FILE__, __LINE__).stream()

// 无论在什么情况下都输出
#define LOG_WARN  Logger(__FILE__, __LINE__,  Logger::WARN).stream()
#define LOG_ERROR  Logger(__FILE__, __LINE__,  Logger::ERROR).stream()
#define LOG_FATAL  Logger(__FILE__, __LINE__,  Logger::FATAL).stream()
#define LOG_SYSERR  Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL  Logger(__FILE__, __LINE__, true).stream()