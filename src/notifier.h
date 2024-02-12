#pragma once

#include <chrono>
#include <condition_variable>

class Notifier
{
  public:
    inline void notify() { cv.notify_one(); }

    template <class Rep, class Period> std::cv_status wait_for(const std::chrono::duration<Rep, Period> &duration)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, duration);
    }

  private:
    std::mutex mutex;
    std::condition_variable cv;
};
