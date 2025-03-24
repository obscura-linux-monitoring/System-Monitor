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

class DiskCollector : public Collector
{
private:
    vector<DiskInfo> disk_stats;
    chrono::steady_clock::time_point last_collect_time;
    unordered_map<string, string> device_name_cache;
    mutable mutex disk_stats_mutex;

    // 스레드 풀 관련 멤버 변수
    const size_t THREAD_POOL_SIZE = 4; // 스레드 풀 크기
    vector<thread> thread_pool;
    queue<function<void()>> task_queue;
    mutex queue_mutex;
    condition_variable cv;
    atomic<bool> stop_threads{false};
    atomic<int> active_tasks{0};

    void collectDiskInfo();
    void updateDiskUsage();
    void updateIoStats();
    string extractDeviceName(const string &device_path);
    bool detectDiskChanges();
    string getParentDisk(const string &partition);

    // 스레드 풀 관련 메서드
    void threadWorker();
    void addTask(function<void()> task);
    void waitForTasks();
    vector<future<void>> pending_tasks;
    chrono::milliseconds task_timeout{300}; // 태스크 타임아웃 시간

public:
    DiskCollector();
    virtual ~DiskCollector();
    void collect() override;
    vector<DiskInfo> getDiskStats() const;
    void setTaskTimeout(chrono::milliseconds timeout) { task_timeout = timeout; }
};