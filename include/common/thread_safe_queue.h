#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace std;

/**
 * @brief 스레드 안전 큐 구현
 *
 * 여러 스레드에서 안전하게 사용할 수 있는 큐 자료구조
 * 생산자-소비자 패턴에 적합하며 동기화 메커니즘이 내장되어 있다
 *
 * @tparam T 큐에 저장될 요소의 타입
 */
template <typename T>
class ThreadSafeQueue
{
public:
    /**
     * @brief 스레드 안전 큐 생성자
     *
     * @param max_size 큐의 최대 크기 (기본값: 100)
     */
    ThreadSafeQueue(size_t max_size = 100) : max_size_(max_size), should_terminate_(false) {}

    /**
     * @brief 큐에 아이템 추가 (생산자용)
     *
     * 큐가 가득 찼을 경우 공간이 생길 때까지 대기한다
     * 종료 신호가 발생한 경우 작업을 중단한다
     *
     * @param item 큐에 추가할 아이템
     * @return bool 성공 여부 (true: 성공, false: 종료 신호 수신됨)
     */
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

    /**
     * @brief 큐에서 아이템 가져오기 (소비자용)
     *
     * 큐가 비었을 경우 아이템이 추가될 때까지 대기한다
     * 종료 신호가 발생하고 큐가 비어있는 경우 작업을 중단한다
     *
     * @param item 가져온 아이템을 저장할 참조 변수
     * @return bool 성공 여부 (true: 성공, false: 종료 신호 수신됨)
     */
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

    /**
     * @brief 종료 신호 발생
     *
     * 현재 대기 중인 모든 스레드에게 종료 신호를 전달한다
     */
    void terminate()
    {
        lock_guard<mutex> lock(mutex_);
        should_terminate_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    /**
     * @brief 큐가 비어있는지 확인
     *
     * @return bool 큐가 비어있으면 true, 그렇지 않으면 false
     */
    bool empty() const
    {
        lock_guard<mutex> lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief 현재 큐에 저장된 아이템 개수 반환
     *
     * @return size_t 큐에 저장된 아이템 개수
     */
    size_t size() const
    {
        lock_guard<mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable mutex mutex_;           ///< 큐 접근 동기화를 위한 뮤텍스
    condition_variable not_empty_;  ///< 큐가 비어있지 않음을 알리는 조건 변수
    condition_variable not_full_;   ///< 큐가 가득 차지 않음을 알리는 조건 변수
    queue<T> queue_;                ///< 실제 데이터가 저장되는 내부 큐
    size_t max_size_;               ///< 큐의 최대 크기
    atomic<bool> should_terminate_; ///< 종료 신호 플래그
};
