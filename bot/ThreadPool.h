#ifndef FRIENDS_TRIP_BOT_THREADPOOL_H
#define FRIENDS_TRIP_BOT_THREADPOOL_H

#include <cstddef>
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace bot {

class ThreadPool {
public:
    ThreadPool(std::size_t numWorkers, std::size_t maxQueueSize);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueue a task. Blocks if the queue is at capacity (backpressure).
    // Returns false if the pool has been shut down.
    bool submit(std::function<void()> task);

    // Initiate shutdown: no new tasks accepted, workers drain the queue then exit.
    void shutdown();

    // Block until all workers have finished.
    void waitForDrain();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> queue_;
    std::size_t maxQueueSize_;

    std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;

    bool stopped_ = false;
    bool drained_ = false;

    void workerLoop();
};

} // namespace bot

#endif // FRIENDS_TRIP_BOT_THREADPOOL_H
