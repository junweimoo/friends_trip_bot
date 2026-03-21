#ifndef FRIENDS_TRIP_BOT_SCHEDULER_H
#define FRIENDS_TRIP_BOT_SCHEDULER_H

#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace bot {

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    void registerTask(std::function<void()> task, bool isRecurring, int hh, int mm, int ss);
    void startWorker();
    void stop();

private:
    struct ScheduledTask {
        std::function<void()> task;
        bool recurring;
        int hh;
        int mm;
        int ss;
    };

    std::vector<ScheduledTask> tasks;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> running{false};
    std::thread workerThread;
    std::time_t lastCheckTime{0};

    void workerLoop();
};

}

#endif //FRIENDS_TRIP_BOT_SCHEDULER_H