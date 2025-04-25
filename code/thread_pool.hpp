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

class FixedThreadPool
{
public:
    // 外部传入线程数,任务队列大小
    explicit FixedThreadPool(size_t thread_count, size_t max_queue_size)
        : stop(false), max_queue_size(max_queue_size)
    {
        // 根据线程数量构建线程
        for (size_t i = 0; i < thread_count; ++i)
        {
            workers.emplace_back([this]
                                 {
                for (;;) {
                    Task task;

                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        cond_var.wait(lock, [this] {
                            return stop || !tasks.empty();
                        });

                        if (stop && tasks.empty()) return;

                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    task();
                } });
        }
    }

    // 提交任务
    template <class F, class... Args>
    auto submit(F &&f, Args &&...args)
        -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        Task wrapper = [task_ptr]()
        { (*task_ptr)(); };

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (tasks.size() >= max_queue_size)
            {
                throw std::runtime_error("Task queue is full. Rejecting task.");
            }
            tasks.emplace(std::move(wrapper));
        }

        cond_var.notify_one();
        return task_ptr->get_future();
    }

    ~FixedThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cond_var.notify_all();
        for (std::thread &worker : workers)
            worker.join();
    }

private:
    using Task = std::function<void()>;

    std::vector<std::thread> workers;
    std::queue<Task> tasks;

    std::mutex queue_mutex;
    std::condition_variable cond_var;
    bool stop;

    size_t max_queue_size;
};
