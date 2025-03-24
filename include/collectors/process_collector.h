#pragma once
#include "collector.h"
#include "../models/process_info.h"
#include <vector>
#include <map>
#include <string>
#include <string_view>
#include <functional>

using namespace std;

class ProcessCollector : public Collector
{
private:
    alignas(64) vector<ProcessInfo> processes;
    alignas(64) map<pid_t, pair<unsigned long, unsigned long>> prev_cpu_times;
    alignas(64) unsigned long prev_total_time = 0;
    vector<ProcessInfo> getProcessesByCpuUsage() const;
    vector<ProcessInfo> getProcessesByMemoryUsage() const;
    vector<ProcessInfo> getProcessesByPid() const;
    vector<ProcessInfo> getProcessesByName() const;
    template <typename Comparator>
    vector<ProcessInfo> getSortedProcesses(Comparator comp) const;
    string convertStatus(string_view status) const;
    vector<ProcessInfo> getTopProcessesByCpu(size_t count) const;
    vector<ProcessInfo> getTopProcessesByMemory(size_t count) const;
    vector<ProcessInfo> getTopProcessesByPid(size_t count) const;
    vector<ProcessInfo> getTopProcessesByName(size_t count) const;

public:
    static const int MAX_SORT_BY = 3;
    void collect() override;
    vector<ProcessInfo> getProcesses(int sort_by) const;
    vector<ProcessInfo> getTopProcesses(int sort_by, size_t count) const;
    bool killProcess(pid_t pid);
};