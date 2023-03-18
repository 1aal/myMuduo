#include "base/LogFile.h"
#include <thread>
#include <assert.h>

LogFlie::LogFlie(const char *basename, off_t rollsize, bool threadSafe, int flushInterval, int checkeveryN) : basename_(basename),
                                                                                                              rollsize_(rollsize),
                                                                                                              flushInterval_(flushInterval),
                                                                                                              checkEveryN_(checkeveryN),
                                                                                                              count_(0),
                                                                                                              startOfPeriod_(0),
                                                                                                              lastRoll_(0),
                                                                                                              lastFlush_(0),
                                                                                                              mutex_(threadSafe ? new mutex : nullptr)
{
    assert(basename_.find('/') == string::npos);
    rollfile();
}

bool LogFlie::rollfile()
{
    time_t now = 0;
    string filename = getLogfileName(basename_, &now);
    time_t start = now / kRollSeconds * kRollSeconds; // start是当天时间零时 now是当天准确时刻
    if (now > lastRoll_)
    {
        lastRoll_ = now,
        lastFlush_ = now;
        startOfPeriod_ = start; // 记录条log属于哪一天
        file_.reset(new AppendFile(filename));
        return true;
    }
    return false;
}

string LogFlie::getLogfileName(const string &basename, time_t *now)
{
    string filename;
    // vector的reserve增加了vector的capacity，但是它的size没有改变！而resize改变了vector的capacity同时也增加了它的size！
    // reserve后的容器只能用push_back添加 因为此时容器类还没有真正的空间
    filename.reserve(filename.size() + 64);
    filename = basename;

    char timebuf[32];
    tm tm;
    *now = time(NULL);
    gmtime_r(now, &tm);

    strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
    filename += timebuf;

    // filename += ProcessInfo::hostname();
    char pidbuf[32];
    snprintf(pidbuf, sizeof pidbuf, ".%d", this_thread::get_id());
    filename += pidbuf;
    filename += ".log";
    return filename;
}

void LogFlie::append_unlock(const char *logline, int len)
{
    file_->append(logline, len);
    if (file_->writtenBytes() > rollsize_)
    {
        rollfile();
    }
    else
    {
        ++count_; // 记录写次数+1
        if (count_ >= checkEveryN_)
        { // 每写三次 检查一下上一次刷新的时间距离现在的时间是否大于flushInterval_ 若大于 则刷新 检查一下是否到达第二天
            count_ = 0;
            time_t now = time(NULL); // time_t是秒数的整数值
            time_t thisPeriod_ = now / kRollSeconds * kRollSeconds;
            if (thisPeriod_ != startOfPeriod_)
            {
                rollfile();
            }
            else if (now - lastFlush_ > flushInterval_)
            {
                lastFlush_ = now;
                file_->flush();
            }
        }
    }
}

void LogFlie::append(const char *logline, int len)
{
    if(mutex_){
        lock_guard<mutex> lc(*mutex_);
        append_unlock(logline,len);
    }else{
        append_unlock(logline,len);
    }
}

void LogFlie::flush()
{
    if (mutex_)
    {
        lock_guard<mutex> lock(*mutex_);
        file_->flush();
    }
    else
    {
        file_->flush();
    }
}
