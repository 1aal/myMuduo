#include "mynet/EventLoopThreadPool.h"
#include "mynet/EventLoop.h"
#include "base/Logger.h"

#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <assert.h>
using namespace std;


void print(EventLoop* p = NULL)
{
  printf("main(): pid = %d, tid = %lld, loop = %p\n",
         getpid(), this_thread::get_id(), p);
}

void init(EventLoop* p)
{
  printf("init(): pid = %d, tid = %lld, loop = %p\n",
         getpid(), this_thread::get_id(), p);
}

int main()
{
Logger::setLogLevel(Logger::ERROR);

  print();

  EventLoop loop;
  loop.runAfter(11, std::bind(&EventLoop::quit, &loop));

  {
    printf("Single thread %p:\n", &loop);
    EventLoopThreadPool model(&loop, "single");
    model.setThreadNum(0);
    model.start(init);
    assert(model.getNextLoop() == &loop);
    assert(model.getNextLoop() == &loop);
    assert(model.getNextLoop() == &loop);
  }

  {
    printf("Another thread:\n");
    EventLoopThreadPool model(&loop, "another");
    model.setThreadNum(1);
    model.start(init);
    EventLoop* nextLoop = model.getNextLoop();
    nextLoop->runAfter(2, std::bind(print, nextLoop));
    assert(nextLoop != &loop);
    assert(nextLoop == model.getNextLoop());
    assert(nextLoop == model.getNextLoop());
    ::sleep(3);
  }

  {
    printf("Three threads:\n");
    EventLoopThreadPool model(&loop, "three");
    model.setThreadNum(3);
    model.start(init);
    EventLoop* nextLoop = model.getNextLoop();
    nextLoop->runInLoop(std::bind(print, nextLoop));
    assert(nextLoop != &loop);
    assert(nextLoop != model.getNextLoop());
    assert(nextLoop != model.getNextLoop());
    assert(nextLoop == model.getNextLoop());
  }

  loop.loop();
}
