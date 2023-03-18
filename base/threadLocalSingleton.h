// 已有 c++11 关键字 实现 thread_local; 线程私有的全局变量，仅在某个线程中有效，但却可以跨多个函数访问
// thread_local int a ;
// 代表每个线程中都会拥有一个独立的a变量且互不影响
// 类内的thread_local变量在某个独立的线程中表现为static变量
// https://zhuanlan.zhihu.com/p/77585472

// SingletonThreadLoacl :每个线程中独自拥有的单列对象  作用应该是在同一个线程里可能会调用多个函数 函数间实现一个单例类
// 简单来说就是 利用thread_local实现单线程中的单例模式
#include "base/Noncopyable.h"
#include <thread>
#include <memory>


template <typename T>
class ThreadLocalSingleton : public noncopyable
{

private:
    static thread_local std::unique_ptr<T> value_;
    ThreadLocalSingleton(/* args */);
    ~ThreadLocalSingleton(); // 单例模式 程序运行期间不需要释放

    static void init()
    {
        value_ = std::unique_ptr<T>(new T());
    }

public:
    static T &instance()
    {
        if (!value_)
        {
            init();
        }
        return *value_;
        // static T value_;  简单的使用一个静态局部变量来实现线程安全的单例模式
        // return value_
    }
};

template <class T>
thread_local std::unique_ptr<T> ThreadLocalSingleton<T>::value_;