#pragma once
#include "collectors/cpu_collector.h"
#include "collectors/memory_collector.h"
#include "collectors/disk_collector.h"
#include "collectors/network_collector.h"
#include "collectors/systeminfo_collector.h"
#include "collectors/process_collector.h"
#include "collectors/docker_collector.h"
#include "i_dashboard.h"
#include "models/system_metrics.h"
#include <memory>

extern volatile bool running; // 전역 변수 선언 유지

class DashboardNcurses : public IDashboard
{
public:
    DashboardNcurses();
    ~DashboardNcurses() override;
    void init() override;
    void update() override;
    void handleInput() override;
    void cleanup() override;

private:
    int row = 0;
    int view_info = 0;
    size_t current_page = 0;
    size_t processes_per_page = 0;
    int sort_by = 0;
    void clearScreen();

    SystemMetrics system_metrics;

    // collector 멤버 변수들 추가
    CPUCollector cpu_collector;
    MemoryCollector memory_collector;
    DiskCollector disk_collector;
    NetworkCollector network_collector;
    SystemInfoCollector systeminfo_collector;
    ProcessCollector process_collector;
    DockerCollector docker_collector;

    void updateMonitor();
    void updateSystemInfo();
    void updateProcesses();
    void updateDocker();

    std::string getDivider(const std::string &title = "");
};