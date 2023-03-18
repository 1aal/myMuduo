#pragma once

#include<queue>
#include<condition_variable>
using namespace std;

    template <class T>
    class BlockingQueue
    {
    private:
        /* data */
        mutable mutex  _mutex;
        condition_variable _notEmpty;
        queue<T> _queue;    
    public:
        BlockingQueue(/* args */) = default;
        ~BlockingQueue() = default;

        void put(const T& x){
            unique_lock<mutex> lc(_mutex);
            _queue.push(x);
            _notEmpty.notify_one();
        }

        void put(T&& x){
            unique_lock<mutex> lc(_mutex);
            _queue.push(x);
            _notEmpty.notify_one();
        }

        T take(){
            unique_lock<mutex> lc(_mutex);
            _notEmpty.wait(lc,[this](){
                return !this->_queue.empty();
            });
            auto ret = _queue.front();
            _queue.pop();
            return ret;
        }

        size_t size() const{
            lock_guard<mutex> lc(_mutex);
            return _queue.size();
        }

    };




