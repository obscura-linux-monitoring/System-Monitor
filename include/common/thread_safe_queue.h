#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace std;

template <typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue(size_t max_size = 100) : max_size_(max_size), should_terminate_(false) {}

    // 데이터 추가 (생산자용)
    bool push(T item)
    {
        unique_lock<mutex> lock(mutex_);

        // 큐가 가득 찼을 때 대기
        not_full_.wait(lock, [this]
                       { return queue_.size() < max_size_ || should_terminate_; });

        if (should_terminate_)
            return false;

        queue_.push(move(item));

        // 소비자에게 데이터가 있음을 알림
        lock.unlock();
        not_empty_.notify_one();

        return true;
    }

    // 데이터 가져오기 (소비자용)
    bool pop(T &item)
    {
        unique_lock<mutex> lock(mutex_);

        // 큐가 비었을 때 대기
        not_empty_.wait(lock, [this]
                        { return !queue_.empty() || should_terminate_; });

        if (should_terminate_ && queue_.empty())
            return false;

        item = move(queue_.front());
        queue_.pop();

        // 생산자에게 공간이 있음을 알림
        lock.unlock();
        not_full_.notify_one();

        return true;
    }

    // 종료 신호
    void terminate()
    {
        lock_guard<mutex> lock(mutex_);
        should_terminate_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    bool empty() const
    {
        lock_guard<mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const
    {
        lock_guard<mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable mutex mutex_;
    condition_variable not_empty_;
    condition_variable not_full_;
    queue<T> queue_;
    size_t max_size_;
    atomic<bool> should_terminate_;
};
