#include "collectors/collector_manager.h"
#include <algorithm>
#include <iostream>
#include <map>

using namespace std;

CollectorManager::CollectorManager(const string &systemKey, size_t queueSize)
    : systemKey_(systemKey), dataQueue_(queueSize), running_(false)
{
}

CollectorManager::~CollectorManager()
{
    stop();
}

void CollectorManager::start(int intervalSeconds)
{
    if (running_)
        return;

    running_ = true;
    collectionThread_ = thread(&CollectorManager::collectLoop, this, intervalSeconds);
}

void CollectorManager::stop()
{
    if (!running_)
        return;

    running_ = false;

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

void CollectorManager::collectLoop(int intervalSeconds)
{
    while (running_)
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

// CPU 수집기에 대한 특수화
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

// 메모리 수집기에 대한 특수화
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

// 디스크 수집기에 대한 특수화
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

// 네트워크 수집기에 대한 특수화
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

// 프로세스 수집기에 대한 특수화
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

// 시스템 정보 수집기에 대한 특수화
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

// 도커 수집기에 대한 특수화
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
    cout << "[수집] 시작: " << startTimestamp
         << ", 종료: " << getCurrentTime()
         << ", 소요 시간: " << collectDuration.count() << "ms" << endl;
}

string CollectorManager::getCurrentTime()
{
    auto now = chrono::system_clock::now();
    auto now_time_t = chrono::system_clock::to_time_t(now);
    auto now_tm = *gmtime(&now_time_t); // UTC 시간 사용

    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &now_tm);
    return string(buffer);
}