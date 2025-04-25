#pragma once

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <stdexcept>
#include <atomic>

static const int thread_count = 4;
static const int max_queue_size = 10;
class FixedThreadPool
{
public:
    // 获取线程池的唯一实例
    static FixedThreadPool &get_instance()
    {
        static FixedThreadPool instance(thread_count, max_queue_size);
        return instance;
    }
    // 提交任务
    template <class F>
    std::future<void> submit(F &&f)
    {
        // packaged_task封装了一个可调用对象,提交异步任务并通过 future 来获取结果
        auto task_ptr = std::make_shared<std::packaged_task<void()>>(std::forward<F>(f));
        {
            std::unique_lock<std::mutex> lock(mtx_);
            // 阻塞直到队列有空间
            cond_.wait(lock, [this]
                       {
                           return tasks_.size() < max_queue_size_ || stop_; // 等待直到队列未满或停止标志被设置
                       });
            // 如果停止标志被设置，则不再接受任务
            if (stop_)
            {
                throw std::runtime_error("ThreadPool is stopping. Cannot accept new tasks.");
            }
            // 将一个 任务（即 std::packaged_task）包装成一个可执行的 lambda，并将这个 lambda 放入到任务队列 tasks_ 中
            tasks_.emplace([task_ptr]()
                           { (*task_ptr)(); });
        }
        cond_.notify_one();
        // 在任务执行完成后通过 std::future 获取任务的结果或状态
        return task_ptr->get_future();
    }
    ~FixedThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            stop_ = true;
        }
        // 唤醒所有线程，让它们检查停止标志并退出
        cond_.notify_all();
        for (std::thread &worker : workers_)
            worker.join();
    }

private:
    // 外部传入线程数,任务队列大小
    explicit FixedThreadPool(size_t thread_count, size_t max_queue_size)
        : stop_(false), max_queue_size_(max_queue_size)
    {
        // 根据线程数量构建线程
        for (size_t i = 0; i < thread_count; ++i)
        {
            workers_.emplace_back([this]
                                  {
                while(true) {
                    Task task;
                    //大括号用于控制锁的范围
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        // 当线程池启动/队列中有任务时,被唤醒
                        cond_.wait(lock, [this] {
                            return stop_ || !tasks_.empty();
                        });

                        //在外部想要关闭线程池+任务处理完毕后,线程退出
                        if (stop_ && tasks_.empty()) return;

                        task = std::move(tasks_.front());
                        tasks_.pop();
                     }
                    task();
                } });
        }
    }
    // 禁止拷贝构造和赋值操作
    FixedThreadPool(const FixedThreadPool &) = delete;
    FixedThreadPool &operator=(const FixedThreadPool &) = delete;

    using Task = std::function<void()>;

    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;

    std::mutex mtx_;
    std::condition_variable cond_;
    bool stop_;

    size_t max_queue_size_;
};
