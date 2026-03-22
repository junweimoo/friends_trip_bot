#include "ThreadPool.h"
#include <spdlog/spdlog.h>

namespace bot {

ThreadPool::ThreadPool(std::size_t numWorkers, std::size_t maxQueueSize)
    : maxQueueSize_(maxQueueSize) {
    workers_.reserve(numWorkers);
    for (std::size_t i = 0; i < numWorkers; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    waitForDrain();
}

bool ThreadPool::submit(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [this] {
            return stopped_ || queue_.size() < maxQueueSize_;
        });
        if (stopped_) return false;
        queue_.push(std::move(task));
    }
    notEmpty_.notify_one();
    return true;
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    notEmpty_.notify_all();
    notFull_.notify_all();
}

void ThreadPool::waitForDrain() {
    shutdown();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (drained_) return;
        drained_ = true;
    }
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            notEmpty_.wait(lock, [this] {
                return stopped_ || !queue_.empty();
            });
            if (stopped_ && queue_.empty()) return;
            task = std::move(queue_.front());
            queue_.pop();
        }
        notFull_.notify_one();
        try {
            task();
        } catch (const std::exception& e) {
            spdlog::error("ThreadPool worker caught exception: {}", e.what());
        } catch (...) {
            spdlog::error("ThreadPool worker caught unknown exception");
        }
    }
}

} // namespace bot
