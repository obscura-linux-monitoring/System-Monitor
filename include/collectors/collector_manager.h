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

/**
 * @brief 시스템 모니터링을 위한 데이터 수집기 관리 클래스
 *
 * 여러 종류의 시스템 데이터 수집기를 관리하고 주기적으로 데이터를 수집하는
 * 매니저 클래스입니다. 병렬 처리를 통해 효율적으로 여러 시스템 메트릭을
 * 동시에 수집합니다.
 */
class CollectorManager
{
public:
    /**
     * @brief CollectorManager 생성자
     *
     * @param systemKey 시스템 식별을 위한 고유 키 값
     * @param queueSize 수집된 데이터를 저장할 큐의 최대 크기 (기본값: 50)
     */
    CollectorManager(const string &systemKey, size_t queueSize = 50);

    /**
     * @brief CollectorManager 소멸자
     *
     * 객체 소멸 시 실행 중인 모든 수집 작업을 중지합니다.
     */
    ~CollectorManager();

    /**
     * @brief 시스템 데이터 수집 작업 시작
     *
     * @param intervalSeconds 데이터 수집 주기(초 단위, 기본값: 5)
     */
    void start(int intervalSeconds = 5);

    /**
     * @brief 시스템 데이터 수집 작업 중지
     *
     * 현재 실행 중인 모든 수집 작업을 안전하게 중지합니다.
     */
    void stop();

    /**
     * @brief 수집된 데이터 큐에 대한 접근자
     *
     * @return ThreadSafeQueue<SystemMetrics>& 수집된 시스템 메트릭 데이터가 저장된 쓰레드 안전 큐
     */
    ThreadSafeQueue<SystemMetrics> &getDataQueue() { return dataQueue_; }

private:
    /**
     * @brief CPU 사용량 수집기 인스턴스
     */
    CPUCollector cpuCollector_;

    /**
     * @brief 메모리 사용량 수집기 인스턴스
     */
    MemoryCollector memoryCollector_;

    /**
     * @brief 디스크 사용량 수집기 인스턴스
     */
    DiskCollector diskCollector_;

    /**
     * @brief 네트워크 트래픽 수집기 인스턴스
     */
    NetworkCollector networkCollector_;

    /**
     * @brief 프로세스 정보 수집기 인스턴스
     */
    ProcessCollector processCollector_;

    /**
     * @brief 시스템 기본 정보 수집기 인스턴스
     */
    SystemInfoCollector systemInfoCollector_;

    /**
     * @brief 도커 컨테이너 정보 수집기 인스턴스
     */
    DockerCollector dockerCollector_;

    /**
     * @brief 서비스 상태 수집기 인스턴스
     */
    ServiceCollector serviceCollector_;

    /**
     * @brief 시스템 식별 키
     */
    string systemKey_;

    /**
     * @brief 수집된 데이터를 저장하는 쓰레드 안전 큐
     */
    ThreadSafeQueue<SystemMetrics> dataQueue_;

    /**
     * @brief 수집 작업 실행 상태 플래그
     */
    atomic<bool> running_;

    /**
     * @brief 주 수집 작업 쓰레드
     */
    thread collectionThread_;

    /**
     * @brief 개별 수집기 작업 상태를 관리하는 future 객체 목록
     */
    vector<future<void>> collectorTasks_;

    /**
     * @brief 쓰레드 풀 (future 객체 정리용)
     */
    vector<future<void>> threadPool_;

    /**
     * @brief 주기적인 데이터 수집 루프 실행 함수
     *
     * @param intervalSeconds 수집 주기(초 단위)
     */
    void collectLoop(int intervalSeconds);

    /**
     * @brief 병렬로 여러 수집기의 데이터를 동시에 수집하는 함수
     */
    void collectDataParallel();

    /**
     * @brief 현재 시간 정보를 문자열로 생성하는 함수
     *
     * @return string 현재 시간 문자열
     */
    string getCurrentTime();

    /**
     * @brief 단일 수집기의 작업을 생성하는 템플릿 함수
     *
     * @tparam Collector 수집기 타입
     * @param collector 수집기 인스턴스 참조
     * @param metrics 수집된 데이터를 저장할 메트릭 객체
     * @param metricsMutex 메트릭 객체 접근을 동기화할 뮤텍스
     * @return function<void()> 수집 작업 함수 객체
     */
    template <typename Collector>
    function<void()> createCollectorTask(Collector &collector, SystemMetrics &metrics, mutex &metricsMutex);
};
