/**
 * 提供日志滚动功能 线程安全写文件功能 线程安全flush功能
*/

#include<memory>
#include<time.h>
#include<string>
#include<mutex>
#include "base/LogStream.h"
#include "base/FileUtils.h"
#include "base/Noncopyable.h"
using namespace std;
class AppendFile;
class LogFlie : noncopyable
{
private:
    void append_unlock(const char* logline,int len);

    static string getLogfileName(const string& basename,time_t *now);

    const string basename_;
    const off_t  rollsize_;
    const int flushInterval_; //刷新间隔秒数 默认为3秒 即每次检查时若上次刷新大于3s会重新刷新
    const int checkEveryN_; //每写三次检查
    
    int count_; // 写入计次

    std::unique_ptr<mutex> mutex_;
    time_t startOfPeriod_; //log记录当天的0点时间
    time_t lastRoll_;  //上一次Roll时间
    time_t lastFlush_; //上一次刷新的时间
    std::unique_ptr<AppendFile> file_;

    const static int kRollSeconds = 24*60*60; //一天的时间
public:
//定义了一个带默认参数的构造函数，是在声明时指定默认参数，而定义时则可以不指定默认参数
    LogFlie(const char *basename,
    off_t rollsize,
    bool threadSafe = true,
    int flushInterval = 3, //刷新区间
    int checkeveryN = 1024
    );
    ~LogFlie() = default;

    void append(const char* logline,int len);
    void flush();

    bool rollfile(); //创建新的回滚文件

    
};

