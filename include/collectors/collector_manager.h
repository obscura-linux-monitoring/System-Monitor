#pragma once

#include "collectors/cpu_collector.h"
#include "collectors/memory_collector.h"
#include "collectors/disk_collector.h"
#include "collectors/network_collector.h"
#include "collectors/process_collector.h"
#include "collectors/systeminfo_collector.h"
#include "collectors/docker_collector.h"
#include "collectors/service_collector.h"
#include "models/system_metrics.h"
#include "common/thread_safe_queue.h"

#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <condition_variable>

using namespace std;

class CollectorManager
{
public:
    CollectorManager(const string &systemKey, size_t queueSize = 50);
    ~CollectorManager();

    // 수집 시작
    void start(int intervalSeconds = 5);

    // 수집 중지
    void stop();

    // 데이터 큐 접근자
    ThreadSafeQueue<SystemMetrics> &getDataQueue() { return dataQueue_; }

private:
    // 각 수집기 인스턴스
    CPUCollector cpuCollector_;
    MemoryCollector memoryCollector_;
    DiskCollector diskCollector_;
    NetworkCollector networkCollector_;
    ProcessCollector processCollector_;
    SystemInfoCollector systemInfoCollector_;
    DockerCollector dockerCollector_;
    ServiceCollector serviceCollector_;

    // 시스템 식별 키
    string systemKey_;

    // 수집된 데이터 저장 큐
    ThreadSafeQueue<SystemMetrics> dataQueue_;

    // 쓰레드 관리
    atomic<bool> running_;
    thread collectionThread_;
    vector<future<void>> collectorTasks_;

    // 쓰레드 풀 (future 객체 정리용)
    vector<future<void>> threadPool_;

    // 수집 작업 실행 함수
    void collectLoop(int intervalSeconds);

    // 병렬 데이터 수집 함수
    void collectDataParallel();

    // 시간 정보 생성
    string getCurrentTime();

    // 단일 수집 작업 실행 (쓰레드에서 호출)
    template <typename Collector>
    function<void()> createCollectorTask(Collector &collector, SystemMetrics &metrics, mutex &metricsMutex);
};
