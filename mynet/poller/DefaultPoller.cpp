#include "mynet/Poller.h"
#include "mynet/poller/EPollPoller.h"
#include "mynet/poller/PollPoller.h"
#include <stdlib.h>

Poller* Poller::newDefualtPoller(EventLoop* loop){
    return new EPollPoller(loop);
    // if(::getenv("MUDUO_USE_POLL")){
    //     return new PollPoller(loop);
    // }else{
    //     return new EPollPoller(loop);
    // }
}