#pragma once

#include <vector>
#include <condition_variable>
using namespace std;


template<class T>
class BoundedBlockingQueue
{
private:
    mutable mutex mutex_;
    condition_variable not_full_;
    condition_variable not_empty_;
    vector<T> queue_;
    int maxSize_;
    int front_;
    int tail_;

public:   
    BoundedBlockingQueue(/* args */) = default;
    BoundedBlockingQueue(int size = 1):queue_(size+1),maxSize_(size),front_(0),tail_(0){};  //通过空一格来实现支持长度为1的循环队列
    ~BoundedBlockingQueue() = default;

    
    void put(const T  &x)
    {
        unique_lock<mutex> lc(mutex_);
        not_full_.wait(lc,[this](){
            return front_ != (tail_ + 1) % (maxSize_ + 1);
        });
        queue_[tail_] = x;
        tail_ = (tail_ + 1) % (maxSize_+1);
        not_empty_.notify_one();
    }

    
    void put(T&& x)
    {
        unique_lock<mutex> lc(mutex_);
        not_full_.wait(lc,[this](){
            return front_ != (tail_ + 1) % (maxSize_ + 1);
        });
        queue_[tail_] = x;
        tail_ = (tail_ + 1) % (maxSize_+1);
        not_empty_.notify_one();
    }

   
    T take()
    {
        unique_lock<mutex> lc(mutex_);
        not_empty_.wait(lc,[this](){
            return this->front_ != this->tail_;
        });
        auto ret = queue_[front_];
        front_ = (front_ + 1) % (maxSize_ + 1);
        not_full_.notify_one();
        return ret;
    }
    bool isEmpty() const
    {
        lock_guard<mutex> lc(mutex_);     
        return tail_ == front_;
    }

    bool isFull() const{
        lock_guard<mutex> lc(mutex_);     
        return front_ == (tail_ + 1) % (maxSize_ + 1);
    }


    size_t size() const{
         lock_guard<mutex> lc(mutex_);
         return (tail_ - front_  + maxSize_ + 1) % (maxSize_ + 1);
    }  
};



