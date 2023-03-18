#include "base/Logger.h"
#include <sstream>
#include <chrono>
#include <base/Timestamp.h>
using namespace std;
int main(){
    LOG_DEBUG << "hello world";
    LOG_INFO << "alala " << 55;
    LOG_TRACE << 123 << "mlw" << 123;
    
    LOG_DEBUG <<Timestamp::now().toString();
    LOG_DEBUG << this_thread::get_id();
    const std::thread::id threadId_ = this_thread::get_id();
    LOG_DEBUG << "EventLoop created " << " in thread " << threadId_ << " tset";

    return 0;
}