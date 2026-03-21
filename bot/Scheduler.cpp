#include "Scheduler.h"

#include <chrono>
#include <ctime>

namespace bot {

// UTC offset in hours for the timezone that hh:mm:ss values are defined in (GMT+8)
constexpr int TIMEZONE_UTC_OFFSET = 8;

// Tolerance in seconds to account for early wakeups from wait_until
constexpr int WAKE_TOLERANCE_SECS = 2;

Scheduler::Scheduler() = default;

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::registerTask(std::function<void()> task, bool isRecurring, int hh, int mm, int ss) {
    std::lock_guard<std::mutex> lock(mutex);
    tasks.push_back({std::move(task), isRecurring, hh, mm, ss});
}

void Scheduler::startWorker() {
    running = true;
    workerThread = std::thread(&Scheduler::workerLoop, this);
}

void Scheduler::stop() {
    if (running.exchange(false)) {
        cv.notify_all();
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }
}

void Scheduler::workerLoop() {
    // Initialize lastCheckTime to now so the first iteration doesn't fire everything in the past
    std::time_t nowUtc = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    lastCheckTime = nowUtc + TIMEZONE_UTC_OFFSET * 3600;

    while (running) {
        auto now = std::chrono::system_clock::now();
        nowUtc = std::chrono::system_clock::to_time_t(now);
        std::time_t nowTime = nowUtc + TIMEZONE_UTC_OFFSET * 3600 + WAKE_TOLERANCE_SECS;
        std::tm localNow{};
        gmtime_r(&nowTime, &localNow);

        // Find the earliest next fire time across all tasks
        std::chrono::system_clock::time_point nextWake = now + std::chrono::hours(24);

        std::vector<std::function<void()>> toFire;

        {
            std::lock_guard<std::mutex> lock(mutex);
            std::vector<size_t> toRemove;

            for (size_t i = 0; i < tasks.size(); i++) {
                const auto& t = tasks[i];

                // Compute the target time for today
                std::tm target = localNow;
                target.tm_hour = t.hh;
                target.tm_min = t.mm;
                target.tm_sec = t.ss;
                target.tm_isdst = -1;
                std::time_t targetTime = timegm(&target);

                // Also check yesterday's target in case we crossed midnight since lastCheck
                std::time_t yesterdayTarget = targetTime - 86400;

                // Fire if target falls in (lastCheckTime, nowTime]
                bool shouldFire = (targetTime > lastCheckTime && targetTime <= nowTime)
                               || (yesterdayTarget > lastCheckTime && yesterdayTarget <= nowTime);

                if (shouldFire) {
                    toFire.push_back(t.task);
                    if (!t.recurring) {
                        toRemove.push_back(i);
                    }
                }

                // Compute next fire time for sleep calculation
                if (targetTime <= nowTime) {
                    targetTime += 86400;
                }
                auto targetPoint = std::chrono::system_clock::from_time_t(targetTime - TIMEZONE_UTC_OFFSET * 3600);
                if (targetPoint < nextWake) {
                    nextWake = targetPoint;
                }
            }

            // Remove non-recurring tasks that fired
            for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
                tasks[*it] = std::move(tasks.back());
                tasks.pop_back();
            }
        }

        lastCheckTime = nowTime;

        // Fire tasks
        for (const auto& task : toFire) {
            std::thread(task).detach();
        }

        // Wait until the next fire time or until stopped
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait_until(lock, nextWake, [this] { return !running.load(); });
        }
    }
}

}