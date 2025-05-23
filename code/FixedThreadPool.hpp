#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "../code/assistance.hpp"

// 控制标识符
struct exec_controller
{
    bool notify_cancel() { return _should_cancel.exchange(true); }
    bool should_cancel() const { return _should_cancel; }

private:
    std::atomic<bool> _should_cancel{false};
};
// 判断是否被取消
struct exec_context
{
    exec_context(std::shared_ptr<exec_controller> impl)
        : _impl(std::move(impl)) {}
    bool canceled() const { return _impl->should_cancel(); }

private:
    std::shared_ptr<exec_controller> _impl;
};

struct CancellableTask
{
    std::shared_ptr<exec_controller> controller =
        std::make_shared<exec_controller>();
    std::function<void(exec_context)> func;

    void operator()()
    {
        exec_context ctx{controller};
        func(ctx);
    }
};

// --- FixedThreadPool ---

class FixedThreadPool
{
public:
    enum class RejectionPolicy
    {
        BLOCK,
        DISCARD,
        THROW
    };

    explicit FixedThreadPool(size_t thread_count, size_t max_queue_size = 1000,
                             RejectionPolicy policy = RejectionPolicy::BLOCK)
        : max_queue_size_(max_queue_size), reject_policy_(policy), stop_(false)
    {
        for (size_t i = 0; i < thread_count; ++i)
        {
            workers_.emplace_back([this]
                                  {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(mtx_);
            cond_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty())
              return;
            task = std::move(tasks_.front());
            tasks_.pop();
          }
          task();
        } });
        }
    }

    ~FixedThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cond_.notify_all();
        for (std::thread &worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    template <class F>
    std::future<void> submit(const std::string &taskType, F &&f)
    {
        auto timeout = getTimeoutForTask(taskType);
        auto cancellableTask = std::make_shared<CancellableTask>();
        cancellableTask->func = std::forward<F>(f);

        auto taskWrapper = std::make_shared<std::packaged_task<void()>>(
            [cancellableTask]()
            { (*cancellableTask)(); });

        std::future<void> taskFuture = taskWrapper->get_future();

        {
            std::unique_lock<std::mutex> lock(mtx_);

            // 阻塞策略：等待有空间
            if (reject_policy_ == RejectionPolicy::BLOCK)
            {
                cond_.wait(lock,
                           [this]
                           { return tasks_.size() < max_queue_size_ || stop_; });
            }
            // 丢弃策略：抛出异常（不再返回默认构造的 future）
            else if (reject_policy_ == RejectionPolicy::DISCARD)
            {
                if (tasks_.size() >= max_queue_size_)
                {
                    throw std::runtime_error("Task queue is full. Task was discarded.");
                }
            }
            // 异常策略：同样抛出异常
            else if (reject_policy_ == RejectionPolicy::THROW)
            {
                if (tasks_.size() >= max_queue_size_)
                {
                    throw std::runtime_error("Task queue is full.");
                }
            }

            if (stop_)
            {
                throw std::runtime_error(
                    "ThreadPool is stopping. Cannot accept new tasks.");
            }

            tasks_.emplace([taskWrapper]()
                           { (*taskWrapper)(); });
        }

        cond_.notify_one();

        // 启动一个后台线程监控超时取消
        std::thread([taskFuture = std::move(taskFuture),
                     controller = cancellableTask->controller, timeout]() mutable
                    {
      if (taskFuture.wait_for(timeout) == std::future_status::timeout) {
        controller->notify_cancel();
        std::cerr << "[Timeout] Task exceeded time limit and was cancelled.\n";
      } })
            .detach();

        return taskFuture;
    }

private:
    static std::chrono::milliseconds
    getTimeoutForTask(const std::string &taskType)
    {
        if (taskType == ns_helper::TASK_TYPE_BUILD_INDEX)
        {
            return std::chrono::seconds(120);
        }
        else if (taskType == ns_helper::TASK_TYPE_PERSIST_INDEX)
        {
            return std::chrono::seconds(4 * 3600);
        }
        else if (taskType == ns_helper::TASK_TYPE_SEARCH ||
                 taskType == ns_helper::TASK_TYPE_AUTOCOMPLETE)
        {
            return std::chrono::seconds(1);
        }
        else
        {
            return std::chrono::seconds(1);
        }
    }
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cond_;
    bool stop_;
    size_t max_queue_size_;
    RejectionPolicy reject_policy_;
};