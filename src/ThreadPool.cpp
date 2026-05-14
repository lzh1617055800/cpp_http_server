#include "ThreadPool.h"
#include "Logger.h"
using namespace std;

ThreadPool::ThreadPool(int num_threads):stop_(false)
{
    int num = num_threads;
    while(num_threads--)
    {
        threads_.emplace_back(&ThreadPool::workerLoop,this);
    }
    LOG_INFO("线程池启动，工作线程数：" + to_string(num));
}

ThreadPool::~ThreadPool()
{
    {
        unique_lock<mutex> lock(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    for(auto& t :threads_)
    {
        t.join();
    }
    LOG_INFO("线程池已经关闭");
}

void ThreadPool::submit(function<void()> task)
{
    {
        unique_lock<mutex> lock(mtx_);
        task_queue_.push(task);
    }
    cv_.notify_one();
}

void ThreadPool::workerLoop()
{
    while(true)
    {
        function<void()> task;
        {
            unique_lock<mutex> lock(mtx_);
            cv_.wait(lock,[this]()
            {
                return stop_ || !task_queue_.empty();
            });
            if(stop_ == true && task_queue_.empty())
            {
                return;
            }
            task = task_queue_.front();
            task_queue_.pop();
        }
        task();
    }
}
