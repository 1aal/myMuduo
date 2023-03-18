#include "Timer.h"
std::atomic<int64_t> Timer::s_numCreated_;//计时器任务数量

void Timer::restart(Timestamp now)
{
    if(repeat_){ //如果是重复定时器 
        expiration_ = addTime(now,interval_);
    }else{  //如果不是重复定时器 下一超时时刻设为一个非法时间
        expiration_ = Timestamp::invalid();
    }
}
