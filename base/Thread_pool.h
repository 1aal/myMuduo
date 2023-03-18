#ifndef Thread_Pool_H
#define Thread_Pool_H 

#include <functional>
#include <thread>
#include <queue>
#include <vector>

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <future>

/**
 * c++ 11的线程池类
 * 1.任务多态的包装  函数可变模板 bind functional
 * 2.任务返回值的异步获取 future和  packaged_task
 * 3.任务队列的锁和条件变量的通知机制
 * 4.添加指定的线程个数 以及 线程添加发生的时间(构造函数时发生);
 * 5.重点理解 线程由emplace() + lambda表达式创造 每个线程是while + cv.wait 等待任务的commit
 * 6.用原子变量保存 线程池个数 和 线程池状态
 * 
*/
namespace std{

#define Thread_Max_Nums 16

    class Thread_pool
    {
        using Task = function<void()>;

    private:
        /* 基本组件 任务队列 和 线程池 */
        queue<Task> _tasks;
        vector<thread> _pool;  //不支持拷贝构造
        /* 信息组件 */
        unsigned int _poolSize; //池初始化大小
        /* 支持组件*/
        mutex _mutex; //任务队列 锁
        condition_variable _cv;//通知线程的 条件变量  当任务队列中任务数量增加或者关闭线程池时需要通知
        /* 状态组件*/
        atomic<bool> _isStop {false}; //是否停止
        atomic<int> _idleThreadNums; //空闲线程个数

    /***构造和析构函数*/
    public:
        explicit Thread_pool(unsigned int size):_poolSize(size)
        {
            addThread(size);
        };
        ~Thread_pool()
        {
            _isStop = true;
            _cv.notify_all();
            for(auto &_thread : _pool)
            {
                // _thread.detach();//放任线程自生自灭
                if(_thread.joinable())
                {
                    _thread.join(); //等待线程执行完毕
                }
            }
        };

    /***对外暴露接口*/    
    public:
        /**注册执行函数 带返回类型的模板 
         * 将任务包装 并且将包装好的任务传入任务队列
         * F 可调用对象 Args 参数
        */
        template< typename F, typename ... Args>
        auto commit(F&& f, Args&&... args ) -> future<decltype(f(args...))> //模板参数的右值引用 是万能引用 根据具体传进来对象决定是否是右值 
        {
            if( _isStop){
                throw runtime_error("commit on thread pool is stopped.");
            }
            /**
             * 将传入的可调用对象 为可异步调用的function<RetType()> 对象  
            */
            using RetType = decltype(f(args...));
            function<RetType()> callback = bind(forward<F>(f),forward<Args>(args)...);  //函数参数绑定
            //packaged_task是类模板，包装任何可调用 (Callable) 目标，包括函数、 lambda 表达式、 bind 表达式或其他函数对象，
            //使得能异步调用它
            auto task = make_shared<packaged_task<RetType()>>( callback ); //callback可以理解为是packaged_task<RetType()>类的构造函数
            //task是一个指向packaged_task<RetType()>类型的共享指针，make_shared确保任务完成return后释放task的空间

            //异步获得返回值
            future<RetType> future_  = task->get_future();
            
            /**添加到任务队列*/
            {   
                lock_guard<mutex> lock(_mutex);
                _tasks.emplace([task](){
                    (*task)(); //packaged_task对象重载了()启动任务
                });
            }

            _cv.notify_one(); //唤醒一个线程执行
            return future_;
        }   

        /**
         * 返回类型为void的commit函数重载
         */
        void commit( function<void()>& func){
            if( _isStop){
                throw runtime_error("commit on thread pool is stopped.");
            }
            auto task = make_shared<function<void()>>(func);
            {   
                lock_guard<mutex> lock(_mutex);
                _tasks.emplace( [task](){
                    (*task)();
                });
                //_tasks.emplace(forward<function<void()>>(func));
            }
            _cv.notify_one(); //唤醒一个线程执行
        }
        
        int idleCount(){
            return _idleThreadNums;
        }

        int thrCount(){
            return _pool.size();
        }

    private:
        /**
         * 添加指定数量的线程
         * c++11标准库的thread 都是执行一个固定的 task 函数，执行完毕线程也就结束了。如何保证复用线程？
         * 方法是 让每一个 thread 都去执行调度函数：循环获取一个 task，然后执行之
        */
        void addThread( unsigned int addNum){ 
            if(_isStop){
                throw runtime_error("Thread Pool is Stop");
            }
            while( addNum > 0 && _pool.size() < Thread_Max_Nums){                
                _pool.emplace_back([this](){  //调度函数 用lambda来实现 因为不能是左值
                    while (true)
                    {
                        Task task; //一个待执行的任务
                        {
                            unique_lock<mutex> lock_(_mutex); //unique比lock_guard的好处是 unique_lock支持手动调用unlock 和lock                            
                            _cv.wait(lock_,[this]{ //wait直到 task队列不为空或者停止  wait必须配上unique_lock封装的锁
                                return this->_isStop || !this->_tasks.empty();
                            });

                            if(_isStop && this->_tasks.empty())
                            {
                                return ;
                            }

                            -- this->_idleThreadNums;
                            task = move(this->_tasks.front());
                            this->_tasks.pop();
                        }

                        task();//执行
                        ++ this->_idleThreadNums; //原子操作不需要加锁
                    }
                });//end 调度函数              

                //每新增一个线程 _idleThreadNums+1
                ++ this->_idleThreadNums;             
                --addNum;
            }
        }
    };  
}

#endif