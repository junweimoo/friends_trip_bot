#ifndef UTILS_H
#define UTILS_H

#include <chrono>
#include <string>
#include <ctime>
#include <cstdio>
#include <memory>

namespace utils {
    inline std::chrono::system_clock::time_point parseTimestamp(const char* timestampStr) {
        if (timestampStr == nullptr) {
            return std::chrono::system_clock::time_point{};
        }
        int year, month, day, hour, minute, second;
        if (std::sscanf(timestampStr, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
            std::tm tm = {};
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = second;
            tm.tm_isdst = -1;
            std::time_t t = std::mktime(&tm);
            auto tp = std::chrono::system_clock::from_time_t(t);
            return tp;
        }
        return std::chrono::system_clock::time_point{};
    }

    inline std::unique_ptr<char[]> formatTimestamp(const std::chrono::system_clock::time_point& timestamp, int gmtOffsetHours = 0) {
        auto adjustedTime = timestamp + std::chrono::hours(gmtOffsetHours);
        std::time_t t = std::chrono::system_clock::to_time_t(adjustedTime);
        std::tm* tm_ptr = std::gmtime(&t);
        if (!tm_ptr) return nullptr;
        const size_t bufSize = 64;
        auto buffer = std::make_unique<char[]>(bufSize);
        std::strftime(buffer.get(), bufSize, "%d %b %H:%M", tm_ptr);
        return buffer;
    }
}

#endif // UTILS_H
