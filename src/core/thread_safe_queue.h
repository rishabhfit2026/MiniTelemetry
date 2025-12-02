#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::atomic<bool> stopped_{false}; // Flag to signal shutdown

public:
    // Pushes data into the queue
    void push(const T& value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(value);
        }
        cond_var_.notify_one(); // Wake up the consumer
    }

    // Waits for data and pops it. Returns false if the queue is stopped.
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait until queue is not empty OR we are stopped
        cond_var_.wait(lock, [this] { 
            return !queue_.empty() || stopped_; 
        });

        if (stopped_ && queue_.empty()) {
            return false; // Time to shut down
        }

        value = queue_.front();
        queue_.pop();
        return true;
    }

    // Signals all waiting threads to stop
    void stop() {
        stopped_ = true;
        cond_var_.notify_all();
    }
};