#include "base/Logger.h"

#include <thread>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <assert.h>
//----------------------错误码描述----------
thread_local char t_errornbuf[512];
thread_local char t_time[32];
thread_local time_t t_lastSecond;
thread_local char threadid[32];


//通过错误码获得错误信息 调用系统函数strerror_r
const char *strerror_tl(int savedErrorno)
{
    return strerror_r(savedErrorno, t_errornbuf, sizeof t_errornbuf); // Linux系统函数
}

//----------------辅助输出类
class T
{
public:
    T(const char *str, unsigned len)
        : str_(str),
          len_(len)
    {
        assert(strlen(str) == len_);
    }

    const char *str_;
    const unsigned len_;
};

inline LogStream &operator<<(LogStream &s, T v)
{
    s.append(v.str_, v.len_);
    return s;
}

//--------------------初始化日志-------

Logger::LogLevel initLogLevel()
{
    return Logger::TRACE;
}

Logger::LogLevel g_logLevel = initLogLevel();

//-----------设置默认输出和刷新-------
void defaultOutput(const char *msg, int len)
{
    cout << msg;
}
void defaultFlush()
{
    cout << flush;
}

Logger::OutputFunc g_output = defaultOutput; //全局变量 通过设置g_output = func来指定输出的位置
Logger::FlushFunc g_flush = defaultFlush;   //

//----------------------Logger类成员函数实现-------

Logger::Logger(SourceFile file, int line) : impl_(INFO, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level) : impl_(level, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char *func) : impl_(level, 0, file, line)
{
    impl_.stream_ << func << ' ';
}

Logger::Logger(SourceFile file, int line, bool toAbort) : impl_(toAbort ? FATAL : ERROR, errno, file, line)
{
}

Logger::~Logger()
{
    impl_.finish();
    const LogStream::Buffer &buf(stream().Getbuffer());
    g_output(buf.data(), buf.length());
}



Logger::LogLevel Logger::logLevel()
{
    return g_logLevel;
}

void Logger::setLogLevel(LogLevel level)
{
    g_logLevel = level;
}
//--------改为异步日志库时更改g_output和g_flush 指向其他的回调函数
void Logger::setOutput(OutputFunc out)
{
    g_output = out;
}

void Logger::setFlush(FlushFunc flush)
{
    g_flush = flush;
}

LogStream &Logger::stream()
{
    return impl_.stream_;
}

//--------------impl类-----------
const char *LogLevelName[Logger::NUMS_LOG_LEVEL] =
    {
        "TRACE ",
        "DEBUG ",
        "INFO  ",
        "WARN  ",
        "ERROR ",
        "FATAL ",
};

Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile &file, int line)
    : stream_(),
      level_(level),
      line_(line),
      basename_(file),
      time_(chrono::system_clock::now())
{

    /**格式化时间*/
    formatTime();
    /*进程号*/
    
    if( strlen(threadid) == 0 ){
        auto id = this_thread::get_id();
        snprintf(threadid,32," %lld",id);
    }     
    // std::stringstream ss; stringstream效率最低 多线程中不要使用
   
    stream_ << T(threadid,strlen(threadid)) ;
    /*日志等级*/
    stream_ << ' ';
    stream_ << LogLevelName[level];
    /*错误号*/
    if (savedErrno != 0)
    {
        stream_ << strerror_tl(savedErrno) << " (error=" << savedErrno << ")";
    }
}



void Logger::Impl::formatTime()
{
    //
    auto microSecond_since_epoch = time_.microSecondsSinceEpoch();
    time_t seconds = static_cast<time_t>(microSecond_since_epoch/Timestamp::kMicroSecondsPerSecond);
    int micSeconds = static_cast<int>(microSecond_since_epoch%Timestamp::kMicroSecondsPerSecond);
    if(seconds != t_lastSecond){ //如果时间相同不用反复写buff
        t_lastSecond = seconds;
        tm info;
        localtime_r(&seconds,&info);    
        struct DateTime dt(info);
        int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    }
    
    // auto tm_micresec = microSecond_since_epoch % 1000;
    // auto tm_millicsec = microSecond_since_epoch / 1000 % 1000;

    // cout<<strlen(t_time)<<endl;
    // cout<<strlen(t_time)<<endl;
    stream_ << T(t_time, 17) << ' '<<micSeconds<<' ';
    // std::tm* now = gmtime(&last);

    // int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
    //         tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
    //         tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
}
// 重载对SourceFile的 <<
inline LogStream &operator<<(LogStream &s, const SourceFile &v)
{
    s.append(v.data_, v.size_);
    return s;
}

void Logger::Impl::finish()
{   
    //bug:  <<" - " 导致stream_截断 ,检查operator<<(string)的重载 
    stream_ << " - " << basename_ << ":" << line_ << '\n';
}
