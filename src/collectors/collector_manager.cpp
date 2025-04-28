#include "collectors/collector_manager.h"
#include <algorithm>
#include <iostream>
#include <map>
#include "log/logger.h"

using namespace std;

/**
 * @brief 프로그램램 실행 상태를 제어하는 전역 변수
 * @details 모든 스레드에서 안전하게 접근할 수 있는 원자적 변수
 */
extern atomic<bool> running; // extern으로 선언 (참조 기호 & 제거)

/**
 * @brief CollectorManager 생성자
 *
 * @param systemKey 시스템 식별을 위한 고유 키 값
 * @param queueSize 수집된 데이터를 저장할 큐의 최대 크기 (기본값: 50)
 */
CollectorManager::CollectorManager(const string &systemKey, size_t queueSize)
    : systemKey_(systemKey), dataQueue_(queueSize)
{
}

/**
 * @brief CollectorManager 소멸자
 *
 * 객체 소멸 시 실행 중인 모든 수집 작업을 중지합니다.
 */
CollectorManager::~CollectorManager()
{
    stop();
}

/**
 * @brief 시스템 데이터 수집 작업 시작
 *
 * @param intervalSeconds 데이터 수집 주기(초 단위, 기본값: 5)
 */
void CollectorManager::start(int intervalSeconds)
{
    collectionThread_ = thread(&CollectorManager::collectLoop, this, intervalSeconds);
}

/**
 * @brief 시스템 데이터 수집 작업 중지
 *
 * 현재 실행 중인 모든 수집 작업을 안전하게 중지합니다.
 * 쓰레드를 조인하고 남은 작업들을 마무리합니다.
 */
void CollectorManager::stop()
{
    if (collectionThread_.joinable())
    {
        collectionThread_.join();
    }

    // 남은 작업들 정리
    for (auto &task : collectorTasks_)
    {
        if (task.valid())
        {
            task.wait();
        }
    }
    collectorTasks_.clear();
}

/**
 * @brief 주기적인 데이터 수집 루프 실행 함수
 *
 * @param intervalSeconds 수집 주기(초 단위)
 *
 * 지정된 주기마다 데이터를 수집하고, 수집 시간을 고려하여 다음 수집 시간을 조정합니다.
 */
void CollectorManager::collectLoop(int intervalSeconds)
{
    while (running.load())
    {
        auto startTime = chrono::steady_clock::now();

        // 병렬로 데이터 수집
        collectDataParallel();

        // 다음 실행 시간 계산
        auto endTime = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        auto sleepTime = chrono::seconds(intervalSeconds) - elapsed;

        if (sleepTime > chrono::milliseconds(0))
        {
            this_thread::sleep_for(sleepTime);
        }
    }
}

/**
 * @brief 단일 수집기의 작업을 생성하는 템플릿 함수
 *
 * @tparam Collector 수집기 타입
 * @param collector 수집기 인스턴스 참조
 * @param metrics 수집된 데이터를 저장할 메트릭 객체
 * @param metricsMutex 메트릭 객체 접근을 동기화할 뮤텍스
 * @return function<void()> 수집 작업 함수 객체
 *
 * 기본 구현은 수집만 수행하고 메트릭에 저장하지 않습니다.
 * 구체적인 수집기 타입에 대한 특수화를 통해 구현됩니다.
 */
template <typename Collector>
function<void()> CollectorManager::createCollectorTask(
    Collector &collector,
    SystemMetrics &metrics,
    mutex &metricsMutex)
{
    // 기본 구현 - 어떤 수집기인지 알 수 없으므로 단순히 수집만 수행
    return [&collector]()
    {
        collector.collect();
    };
}

/**
 * @brief CPU 수집기에 대한 특수화된 작업 생성 함수
 *
 * @param collector CPU 수집기 인스턴스
 * @param metrics 시스템 메트릭 객체
 * @param metricsMutex 메트릭 접근 동기화를 위한 뮤텍스
 * @return function<void()> CPU 정보 수집 작업 함수
 *
 * CPU 사용량을 수집하고 메트릭 객체에 저장합니다.
 */
template <>
function<void()> CollectorManager::createCollectorTask<CPUCollector>(
    CPUCollector &collector,
    SystemMetrics &metrics,
    mutex &metricsMutex)
{
    return [&collector, &metrics, &metricsMutex]()
    {
        auto startTime = chrono::steady_clock::now();
        collector.collect();

        lock_guard<mutex> lock(metricsMutex);
        metrics.cpu = collector.getCpuInfo();

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        cout << "[CPU 수집] 소요 시간: " << duration.count() << "ms" << endl;
    };
}

/**
 * @brief 메모리 수집기에 대한 특수화된 작업 생성 함수
 *
 * @param collector 메모리 수집기 인스턴스
 * @param metrics 시스템 메트릭 객체
 * @param metricsMutex 메트릭 접근 동기화를 위한 뮤텍스
 * @return function<void()> 메모리 정보 수집 작업 함수
 *
 * 메모리 사용량을 수집하고 메트릭 객체에 저장합니다.
 */
template <>
function<void()> CollectorManager::createCollectorTask<MemoryCollector>(
    MemoryCollector &collector,
    SystemMetrics &metrics,
    mutex &metricsMutex)
{
    return [&collector, &metrics, &metricsMutex]()
    {
        auto startTime = chrono::steady_clock::now();
        collector.collect();

        lock_guard<mutex> lock(metricsMutex);
        metrics.memory = collector.getMemoryInfo();

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        cout << "[메모리 수집] 소요 시간: " << duration.count() << "ms" << endl;
    };
}

/**
 * @brief 디스크 수집기에 대한 특수화된 작업 생성 함수
 *
 * @param collector 디스크 수집기 인스턴스
 * @param metrics 시스템 메트릭 객체
 * @param metricsMutex 메트릭 접근 동기화를 위한 뮤텍스
 * @return function<void()> 디스크 정보 수집 작업 함수
 *
 * 디스크 사용량 통계를 수집하고 메트릭 객체에 저장합니다.
 */
template <>
function<void()> CollectorManager::createCollectorTask<DiskCollector>(
    DiskCollector &collector,
    SystemMetrics &metrics,
    mutex &metricsMutex)
{
    return [&collector, &metrics, &metricsMutex]()
    {
        auto startTime = chrono::steady_clock::now();
        collector.collect();

        lock_guard<mutex> lock(metricsMutex);
        metrics.disk = collector.getDiskStats();

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        cout << "[디스크 수집] 소요 시간: " << duration.count() << "ms" << endl;
    };
}

/**
 * @brief 네트워크 수집기에 대한 특수화된 작업 생성 함수
 *
 * @param collector 네트워크 수집기 인스턴스
 * @param metrics 시스템 메트릭 객체
 * @param metricsMutex 메트릭 접근 동기화를 위한 뮤텍스
 * @return function<void()> 네트워크 정보 수집 작업 함수
 *
 * 네트워크 인터페이스 정보를 수집하고 메트릭 객체에 저장합니다.
 */
template <>
function<void()> CollectorManager::createCollectorTask<NetworkCollector>(
    NetworkCollector &collector,
    SystemMetrics &metrics,
    mutex &metricsMutex)
{
    return [&collector, &metrics, &metricsMutex]()
    {
        auto startTime = chrono::steady_clock::now();
        collector.collect();

        lock_guard<mutex> lock(metricsMutex);
        metrics.network = collector.getInterfacesToVector();

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        cout << "[네트워크 수집] 소요 시간: " << duration.count() << "ms" << endl;
    };
}

/**
 * @brief 프로세스 수집기에 대한 특수화된 작업 생성 함수
 *
 * @param collector 프로세스 수집기 인스턴스
 * @param metrics 시스템 메트릭 객체
 * @param metricsMutex 메트릭 접근 동기화를 위한 뮤텍스
 * @return function<void()> 프로세스 정보 수집 작업 함수
 *
 * 실행 중인 프로세스 정보를 수집하고 메트릭 객체에 저장합니다.
 */
template <>
function<void()> CollectorManager::createCollectorTask<ProcessCollector>(
    ProcessCollector &collector,
    SystemMetrics &metrics,
    mutex &metricsMutex)
{
    return [&collector, &metrics, &metricsMutex]()
    {
        auto startTime = chrono::steady_clock::now();
        collector.collect();

        lock_guard<mutex> lock(metricsMutex);
        metrics.process = collector.getProcesses(0);

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        cout << "[프로세스 수집] 소요 시간: " << duration.count() << "ms" << endl;
    };
}

/**
 * @brief 시스템 정보 수집기에 대한 특수화된 작업 생성 함수
 *
 * @param collector 시스템 정보 수집기 인스턴스
 * @param metrics 시스템 메트릭 객체
 * @param metricsMutex 메트릭 접근 동기화를 위한 뮤텍스
 * @return function<void()> 시스템 정보 수집 작업 함수
 *
 * 기본 시스템 정보를 수집하고 메트릭 객체에 저장합니다.
 */
template <>
function<void()> CollectorManager::createCollectorTask<SystemInfoCollector>(
    SystemInfoCollector &collector,
    SystemMetrics &metrics,
    mutex &metricsMutex)
{
    return [&collector, &metrics, &metricsMutex]()
    {
        auto startTime = chrono::steady_clock::now();
        collector.collect();

        lock_guard<mutex> lock(metricsMutex);
        // 시스템 정보를 메트릭에 추가
        metrics.system = collector.getSystemInfo();

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        cout << "[시스템 정보 수집] 소요 시간: " << duration.count() << "ms" << endl;
    };
}

/**
 * @brief 도커 수집기에 대한 특수화된 작업 생성 함수
 *
 * @param collector 도커 수집기 인스턴스
 * @param metrics 시스템 메트릭 객체
 * @param metricsMutex 메트릭 접근 동기화를 위한 뮤텍스
 * @return function<void()> 도커 컨테이너 정보 수집 작업 함수
 *
 * 도커 컨테이너 정보를 수집하고 메트릭 객체에 저장합니다.
 */
template <>
function<void()> CollectorManager::createCollectorTask<DockerCollector>(
    DockerCollector &collector,
    SystemMetrics &metrics,
    mutex &metricsMutex)
{
    return [&collector, &metrics, &metricsMutex]()
    {
        auto startTime = chrono::steady_clock::now();
        collector.collect();

        lock_guard<mutex> lock(metricsMutex);
        metrics.docker = collector.getContainers();

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        cout << "[도커 수집] 소요 시간: " << duration.count() << "ms" << endl;
    };
}

/**
 * @brief 서비스 수집기에 대한 특수화된 작업 생성 함수
 *
 * @param collector 서비스 수집기 인스턴스
 * @param metrics 시스템 메트릭 객체
 * @param metricsMutex 메트릭 접근 동기화를 위한 뮤텍스
 * @return function<void()> 서비스 정보 수집 작업 함수
 *
 * 시스템 서비스 정보를 수집하고 메트릭 객체에 저장합니다.
 */
template <>
function<void()> CollectorManager::createCollectorTask<ServiceCollector>(
    ServiceCollector &collector,
    SystemMetrics &metrics,
    mutex &metricsMutex)
{
    return [&collector, &metrics, &metricsMutex]()
    {
        auto startTime = chrono::steady_clock::now();
        collector.collect();

        lock_guard<mutex> lock(metricsMutex);
        metrics.services = collector.getServiceInfo();

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        cout << "[서비스 수집] 소요 시간: " << duration.count() << "ms" << endl;
    };
}

/**
 * @brief 병렬로 여러 수집기의 데이터를 동시에 수집하는 함수
 *
 * 모든 수집기를 비동기적으로 실행하여 동시에 데이터를 수집하고,
 * 결과를 하나의 메트릭 객체에 취합합니다.
 * 수집이 완료되면 데이터 큐에 메트릭을 추가합니다.
 */
void CollectorManager::collectDataParallel()
{
    // 시작 시간 기록
    auto collectStartTime = chrono::steady_clock::now();
    string startTimestamp = getCurrentTime();

    SystemMetrics metrics;
    metrics.key = systemKey_;
    metrics.timestamp = startTimestamp;

    // 동시 접근 보호를 위한 뮤텍스
    mutex metricsMutex;

    // 각 수집기에 대한 비동기 작업 생성
    vector<future<void>> tasks;

    // 템플릿 함수를 사용하여 각 수집기에 대한 작업 생성 및 실행
    tasks.push_back(async(launch::async,
                          createCollectorTask(cpuCollector_, metrics, metricsMutex)));

    tasks.push_back(async(launch::async,
                          createCollectorTask(memoryCollector_, metrics, metricsMutex)));

    tasks.push_back(async(launch::async,
                          createCollectorTask(diskCollector_, metrics, metricsMutex)));

    tasks.push_back(async(launch::async,
                          createCollectorTask(networkCollector_, metrics, metricsMutex)));

    tasks.push_back(async(launch::async,
                          createCollectorTask(processCollector_, metrics, metricsMutex)));

    tasks.push_back(async(launch::async,
                          createCollectorTask(systemInfoCollector_, metrics, metricsMutex)));

    tasks.push_back(async(launch::async,
                          createCollectorTask(dockerCollector_, metrics, metricsMutex)));

    tasks.push_back(async(launch::async,
                          createCollectorTask(serviceCollector_, metrics, metricsMutex)));

    // 모든 작업이 완료될 때까지 대기
    for (auto &task : tasks)
    {
        if (task.valid())
        {
            task.wait();
        }
    }

    // 완성된 metrics를 큐에 추가
    dataQueue_.push(move(metrics));

    // 종료 시간 기록 및 소요 시간 계산
    auto collectEndTime = chrono::steady_clock::now();
    auto collectDuration = chrono::duration_cast<chrono::milliseconds>(collectEndTime - collectStartTime);

    // 시작 및 종료 시간, 소요 시간 출력
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "[수집] 시작: %s, 종료: %s, 소요 시간: %ldms", startTimestamp.c_str(), getCurrentTime().c_str(), collectDuration.count());
    cout << buffer << endl;
    LOG_INFO(buffer);
}

/**
 * @brief 현재 시간 정보를 문자열로 생성하는 함수
 *
 * @return string 현재 시간 문자열
 *
 * UTC 시간을 ISO 8601 형식(YYYY-MM-DDThh:mm:ssZ)으로 반환합니다.
 */
string CollectorManager::getCurrentTime()
{
    auto now = chrono::system_clock::now();
    auto now_time_t = chrono::system_clock::to_time_t(now);
    auto now_tm = *gmtime(&now_time_t); // UTC 시간 사용

    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &now_tm);
    return string(buffer);
}