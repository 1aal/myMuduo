#pragma once

#include <mutex>
#include <memory>
#include "base/Noncopyable.h"


/**
 * 基于call once实现的单例模式 保证只创建一次对象 不保证多线程访问对象时不会发生冲突
 * 当T类型对象是线程安全时 保证多线程安全
 * 
 * C++11之后，要求编译器保证内部静态变量的线程安全性，也可以不用加锁解锁，下面的实现是带互斥锁的
*/
template <typename T>
    class Singleton : noncopyable
    {
    private:
        static std::unique_ptr<T> value_;
        static std::once_flag ponce_;

        Singleton(/* args */);
        ~Singleton(); //单例模式 程序运行期间不需要释放

        static void init()
        {
            value_  = std::unique_ptr<T>(new T()) ;
        }

    public:
        static T& instance()
        {   
            std::call_once(ponce_, &init);
            return *value_;
            // static T value_;  简单的使用一个静态局部变量来实现线程安全的单例模式
            // return value_
        }
    };
    
    template <class T>
    std::once_flag Singleton<T>::ponce_;

    template <class T>
    std::unique_ptr<T> Singleton<T>::value_;