#include "mynet/EventLoopThread.h"
#include "mynet/EventLoop.h"
#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <base/Logger.h>
using namespace std;


void print(EventLoop* p = NULL)
{
  printf("print: pid = %d, tid = %lld, loop = %p\n",
         getpid(),this_thread::get_id() , p);
}

void quit(EventLoop* p)
{
  print(p);
  p->quit();
}

int main()
{   
  Logger::setLogLevel(Logger::WARN);  
  print();

  {
    EventLoopThread thr1;  // never start
  }

  {
  // dtor calls quit()
  EventLoopThread thr2;
  EventLoop* loop = thr2.startLoop();
  loop->runInLoop(std::bind(print, loop));
  this_thread::sleep_for(chrono::seconds(5));
//   CurrentThread::sleepUsec(500 * 1000);
  }

  {
  // quit() before dtor
  EventLoopThread thr3;
  EventLoop* loop = thr3.startLoop();
  loop->runInLoop(std::bind(quit, loop)); //从主线程调用EventLoopThread的函数
   this_thread::sleep_for(chrono::seconds(5));
  }
}
