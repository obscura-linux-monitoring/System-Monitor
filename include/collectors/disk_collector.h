#pragma once

#include "collector.h"
#include "models/disk_info.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>

using namespace std;

/**
 * @class DiskCollector
 * @brief 디스크 정보를 수집하고 관리하는 클래스
 *
 * 시스템의 디스크 사용량, I/O 통계 등을 수집하고 분석하는 기능을 제공합니다.
 * 멀티스레드 방식으로 디스크 정보를 효율적으로 수집합니다.
 * @see Collector
 */
class DiskCollector : public Collector
{
private:
    /**
     * @brief 수집된 디스크 정보를 저장하는 벡터
     */
    vector<DiskInfo> disk_stats;

    /**
     * @brief 마지막 정보 수집 시간을 기록하는 타임포인트
     */
    chrono::steady_clock::time_point last_collect_time;

    /**
     * @brief 디바이스 경로와 이름 매핑을 캐싱하는 맵
     */
    unordered_map<string, string> device_name_cache;

    /**
     * @brief disk_stats 접근을 동기화하기 위한 뮤텍스
     */
    mutable mutex disk_stats_mutex;

    // 스레드 풀 관련 멤버 변수
    /**
     * @brief 스레드 풀의 크기
     */
    const size_t THREAD_POOL_SIZE = 4;

    /**
     * @brief 작업을 수행할 스레드 풀
     */
    vector<thread> thread_pool;

    /**
     * @brief 수행할 작업들을 저장하는 큐
     */
    queue<function<void()>> task_queue;

    /**
     * @brief 작업 큐 접근을 동기화하기 위한 뮤텍스
     */
    mutex queue_mutex;

    /**
     * @brief 작업 큐의 상태 변화를 알리는 조건 변수
     */
    condition_variable cv;

    /**
     * @brief 스레드 종료 신호를 위한 플래그
     */
    atomic<bool> stop_threads{false};

    /**
     * @brief 현재 활성화된 작업 수
     */
    atomic<int> active_tasks{0};

    /**
     * @brief 디스크 정보를 수집하는 메서드
     *
     * 시스템의 모든 디스크 정보를 수집하는 프로세스를 시작합니다.
     */
    void collectDiskInfo();

    /**
     * @brief 디스크 사용량을 업데이트하는 메서드
     *
     * 각 디스크의 총 용량, 사용 공간, 여유 공간 등을 업데이트합니다.
     */
    void updateDiskUsage();

    /**
     * @brief 디스크 I/O 통계를 업데이트하는 메서드
     *
     * 읽기/쓰기 작업, 처리량, 대기 시간 등의 I/O 통계를 업데이트합니다.
     */
    void updateIoStats();

    /**
     * @brief 디바이스 경로에서 디바이스 이름을 추출하는 메서드
     *
     * @param device_path 디바이스 경로
     * @return string 추출된 디바이스 이름
     */
    string extractDeviceName(const string &device_path);

    /**
     * @brief 디스크 변경사항 감지 메서드
     *
     * @return bool 디스크 구성이 변경되었으면 true, 아니면 false
     */
    bool detectDiskChanges();

    /**
     * @brief 파티션의 부모 디스크를 찾는 메서드
     *
     * @param partition 파티션 이름 또는 경로
     * @return string 부모 디스크 이름
     */
    string getParentDisk(const string &partition);

    // 스레드 풀 관련 메서드
    /**
     * @brief 스레드 작업자 함수
     *
     * 스레드 풀의 각 스레드가 실행하는 메인 함수로, 작업 큐에서 작업을 가져와 실행합니다.
     */
    void threadWorker();

    /**
     * @brief 작업을 스레드 풀 큐에 추가하는 메서드
     *
     * @param task 실행할 함수 객체
     */
    void addTask(function<void()> task);

    /**
     * @brief 모든 작업이 완료될 때까지 대기하는 메서드
     */
    void waitForTasks();

    /**
     * @brief 대기 중인 비동기 작업들을 저장하는 벡터
     */
    vector<future<void>> pending_tasks;

    /**
     * @brief 작업 타임아웃 설정 (밀리초)
     */
    chrono::milliseconds task_timeout{300};

public:
    /**
     * @brief 기본 생성자
     *
     * 스레드 풀을 초기화하고 디스크 수집기를 설정합니다.
     */
    DiskCollector();

    /**
     * @brief 가상 소멸자
     *
     * 스레드 풀을 정리하고 리소스를 해제합니다.
     */
    virtual ~DiskCollector();

    /**
     * @brief 디스크 정보 수집 메서드
     *
     * Collector 기본 클래스의 collect 메서드를 오버라이드하여 디스크 정보를 수집합니다.
     * @see Collector::collect()
     */
    void collect() override;

    /**
     * @brief 수집된 디스크 통계 정보를 반환하는 메서드
     *
     * @return vector<DiskInfo> 현재 수집된 모든 디스크 정보
     */
    vector<DiskInfo> getDiskStats() const;

    /**
     * @brief 작업 타임아웃 시간을 설정하는 메서드
     *
     * @param timeout 설정할 타임아웃 시간(밀리초)
     */
    void setTaskTimeout(chrono::milliseconds timeout) { task_timeout = timeout; }
};