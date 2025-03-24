#include "collectors/memory_collector.h"
#include <fstream>
#include <sstream>

using namespace std;

void MemoryCollector::clear()
{
    memoryInfo.total = 0;
    memoryInfo.used = 0;
    memoryInfo.free = 0;
    memoryInfo.cached = 0;
    memoryInfo.buffers = 0;
    memoryInfo.available = 0;
    memoryInfo.swap_total = 0;
    memoryInfo.swap_used = 0;
    memoryInfo.swap_free = 0;
    memoryInfo.usage_percent = 0;
}

void MemoryCollector::collect()
{
    ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open())
    {
        throw runtime_error("Cannot open /proc/meminfo");
    }

    // 메모리 정보 초기화
    clear();

    size_t buffers = 0;
    size_t sreclaimable = 0;
    size_t shmem = 0;

    string line;
    while (getline(meminfo, line))
    {
        istringstream iss(line);
        string key;
        size_t value;

        iss >> key >> value;

        if (key == "MemTotal:")
            memoryInfo.total = value * 1024;
        else if (key == "MemFree:")
            memoryInfo.free = value * 1024;
        else if (key == "Cached:")
            memoryInfo.cached = value * 1024;
        else if (key == "Buffers:")
            memoryInfo.buffers = value * 1024;
        else if (key == "SReclaimable:")
            sreclaimable = value * 1024;
        else if (key == "Shmem:")
            shmem = value * 1024;
        else if (key == "SwapTotal:")
            memoryInfo.swap_total = value * 1024;
        else if (key == "SwapFree:")
            memoryInfo.swap_free = value * 1024;
        else if (key == "MemAvailable:")
            memoryInfo.available = value * 1024;
    }

    // btop 스타일의 계산 방식
    size_t cached_total = memoryInfo.cached + memoryInfo.buffers + sreclaimable - shmem;
    memoryInfo.used = memoryInfo.total - memoryInfo.free - cached_total;

    // swap_used 계산
    memoryInfo.swap_used = memoryInfo.swap_total - memoryInfo.swap_free;

    // 메모리 사용 비율 계산 (퍼센트)
    if (memoryInfo.total > 0)
    {
        memoryInfo.usage_percent = (float)memoryInfo.used / memoryInfo.total * 100.0f;
    }
    else
    {
        memoryInfo.usage_percent = 0.0f;
    }
}

MemoryInfo MemoryCollector::getMemoryInfo() const
{
    return memoryInfo;
}