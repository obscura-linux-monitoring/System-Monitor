#include "collectors/process_collector.h"
#include <proc/readproc.h>
#include <signal.h>
#include <cstring>
#include <stdexcept>
#include <pwd.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>
#include <dirent.h>

using namespace std;

string ProcessCollector::convertStatus(string_view status) const
{
    if (status == "R")
        return "Running";
    else if (status == "S")
        return "Sleeping";
    else if (status == "I")
        return "Idle";
    else if (status == "D")
        return "Disk Sleep";
    else if (status == "Z")
        return "Zombie";
    else if (status == "T")
        return "Stopped";
    else if (status == "t")
        return "Tracing Stop";

    return string(status);
}

template <typename Comparator>
vector<ProcessInfo> ProcessCollector::getSortedProcesses(Comparator comp) const
{
    vector<ProcessInfo> result = processes;
    sort(result.begin(), result.end(), comp);
    return result;
}

void ProcessCollector::collect()
{
    processes.clear();
    processes.reserve(prev_cpu_times.size() > 100 ? prev_cpu_times.size() : 100);
    unsigned long total_time = 0;

    // 시스템 부팅 시간 계산
    time_t boot_time = 0;
    {
        // /proc/uptime에서 시스템이 부팅된 후 경과 시간(초)을 읽어옴
        ifstream uptime_file("/proc/uptime");
        double uptime = 0;
        if (uptime_file >> uptime)
        {
            // 현재 시간에서 uptime을 빼서 부팅 시간 계산
            boot_time = time(nullptr) - static_cast<time_t>(uptime);
        }
    }

    // 전체 CPU 시간 계산
    ifstream stat("/proc/stat");
    string line;
    if (getline(stat, line))
    {
        istringstream iss(line);
        string cpu;
        unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
        total_time = user + nice + system + idle + iowait + irq + softirq + steal;
    }

    PROCTAB *proc = openproc(
        PROC_FILLMEM |
        PROC_FILLSTAT |
        PROC_FILLSTATUS |
        PROC_FILLUSR |
        PROC_FILLCOM);

    if (proc == nullptr)
    {
        throw runtime_error("Cannot access process information");
    }

    proc_t proc_info;
    memset(&proc_info, 0, sizeof(proc_info));

    // 사용자 이름을 캐싱하기 위한 맵
    static map<uid_t, string> user_cache;

    while (readproc(proc, &proc_info) != nullptr)
    {
        ProcessInfo process;
        process.pid = proc_info.tid;
        process.ppid = proc_info.ppid;
        process.name = proc_info.cmd;

        // 사용자 이름 캐싱 활용
        auto uid = static_cast<uid_t>(proc_info.euid);
        auto user_it = user_cache.find(uid);
        if (user_it != user_cache.end())
        {
            process.user = user_it->second;
        }
        else
        {
            struct passwd *pw = getpwuid(uid);
            process.user = pw ? pw->pw_name : to_string(uid);
            user_cache[uid] = process.user;
        }

        process.memory_rss = proc_info.vm_rss * 1024;  // memory_usage 대신 memory_rss 사용
        process.memory_vsz = proc_info.vm_size * 1024; // 가상 메모리 크기 추가
        process.threads = proc_info.nlwp;              // 스레드 수

        // 프로세스 시작 시간 설정 (boot_time 사용)
        process.start_time = boot_time + proc_info.start_time / sysconf(_SC_CLK_TCK);

        // CPU 사용량 계산 로직 최적화
        unsigned long process_total_time = proc_info.utime + proc_info.stime;
        auto it = prev_cpu_times.find(process.pid);

        if (it != prev_cpu_times.end() && total_time > prev_total_time)
        {
            unsigned long time_diff = process_total_time - it->second.first;
            unsigned long total_time_diff = total_time - prev_total_time;

            process.cpu_usage = 100.0f * (static_cast<float>(time_diff) / static_cast<float>(total_time_diff));
        }
        else
        {
            process.cpu_usage = 0.0f;
        }

        // CPU 시간 (초 단위)
        process.cpu_time = (proc_info.utime + proc_info.stime) / (float)sysconf(_SC_CLK_TCK);

        prev_cpu_times[process.pid] = make_pair(process_total_time, total_time);

        process.status = convertStatus(&proc_info.state);
        process.command.reserve(128); // 적절한 초기 크기로 예약
        if (proc_info.cmdline)
        {
            for (int i = 0; proc_info.cmdline[i]; i++)
            {
                if (i > 0)
                    process.command += ' ';
                process.command += proc_info.cmdline[i];
            }
        }
        if (process.command.empty())
        {
            process.command = process.name;
        }

        // 추가 필드 설정
        process.nice = proc_info.nice;

        // I/O 통계 및 열린 파일 수 처리 (파일에서 읽어와야 함)
        try
        {
            process.io_read_bytes = 0;
            process.io_write_bytes = 0;
            process.open_files = 0;

            // /proc/{pid}/io에서 I/O 통계 읽기
            ifstream io_file("/proc/" + to_string(process.pid) + "/io");
            if (io_file)
            {
                string io_line;
                while (getline(io_file, io_line))
                {
                    if (io_line.find("read_bytes") != string::npos)
                    {
                        process.io_read_bytes = stoull(io_line.substr(io_line.find(":") + 1));
                    }
                    else if (io_line.find("write_bytes") != string::npos)
                    {
                        process.io_write_bytes = stoull(io_line.substr(io_line.find(":") + 1));
                    }
                }
            }

            // 열린 파일 수 계산 개선
            string fd_path = "/proc/" + to_string(process.pid) + "/fd";
            DIR *dir = opendir(fd_path.c_str());
            if (dir)
            {
                process.open_files = -2; // "." 및 ".." 제외
                while (readdir(dir) != nullptr)
                {
                    process.open_files++;
                }
                closedir(dir);
                if (process.open_files < 0)
                    process.open_files = 0;
            }
        }
        catch (const std::exception &e)
        {
            // 구체적인 예외 처리 및 로깅 (필요시)
            // std::cerr << "Error reading process info for PID " << process.pid << ": " << e.what() << std::endl;
        }

        processes.push_back(process);
    }

    prev_total_time = total_time;
    closeproc(proc);
}

vector<ProcessInfo> ProcessCollector::getProcessesByCpuUsage() const
{
    return getSortedProcesses([](const ProcessInfo &a, const ProcessInfo &b)
                              { return a.cpu_usage > b.cpu_usage; });
}

vector<ProcessInfo> ProcessCollector::getProcessesByMemoryUsage() const
{
    return getSortedProcesses([](const ProcessInfo &a, const ProcessInfo &b)
                              { return a.memory_rss > b.memory_rss; });
}

vector<ProcessInfo> ProcessCollector::getProcessesByPid() const
{
    return getSortedProcesses([](const ProcessInfo &a, const ProcessInfo &b)
                              { return a.pid < b.pid; });
}

vector<ProcessInfo> ProcessCollector::getProcessesByName() const
{
    return getSortedProcesses([](const ProcessInfo &a, const ProcessInfo &b)
                              { return a.name < b.name; });
}

vector<ProcessInfo> ProcessCollector::getProcesses(int sort_by) const
{
    if (sort_by == 0)
        return getProcessesByCpuUsage();
    else if (sort_by == 1)
        return getProcessesByMemoryUsage();
    else if (sort_by == 2)
        return getProcessesByPid();
    else if (sort_by == 3)
        return getProcessesByName();
    return processes;
}

bool ProcessCollector::killProcess(pid_t pid)
{
    return (kill(pid, SIGTERM) == 0);
}

// 메모리 사용량이 많은 프로세스 TOP N만 가져오는 함수 추가
vector<ProcessInfo> ProcessCollector::getTopProcessesByMemory(size_t count) const
{
    vector<ProcessInfo> result = processes;

    // 전체 정렬 대신 부분 정렬 사용 (성능 향상)
    partial_sort(result.begin(),
                 result.begin() + min(count, result.size()),
                 result.end(),
                 [](const ProcessInfo &a, const ProcessInfo &b)
                 {
                     return a.memory_rss > b.memory_rss;
                 });

    // 필요한 크기로 결과 자르기
    if (result.size() > count)
    {
        result.resize(count);
    }

    return result;
}

// CPU 사용량이 많은 프로세스 TOP N만 가져오는 함수 추가
vector<ProcessInfo> ProcessCollector::getTopProcessesByCpu(size_t count) const
{
    vector<ProcessInfo> result = processes;

    partial_sort(result.begin(),
                 result.begin() + min(count, result.size()),
                 result.end(),
                 [](const ProcessInfo &a, const ProcessInfo &b)
                 {
                     return a.cpu_usage > b.cpu_usage;
                 });

    if (result.size() > count)
    {
        result.resize(count);
    }

    return result;
}

vector<ProcessInfo> ProcessCollector::getTopProcessesByPid(size_t count) const
{
    vector<ProcessInfo> result = processes;
    partial_sort(result.begin(),
                 result.begin() + min(count, result.size()),
                 result.end(),
                 [](const ProcessInfo &a, const ProcessInfo &b)
                 {
                     return a.pid < b.pid;
                 });

    if (result.size() > count)
        result.resize(count);

    return result;
}

vector<ProcessInfo> ProcessCollector::getTopProcessesByName(size_t count) const
{
    vector<ProcessInfo> result = processes;
    partial_sort(result.begin(),
                 result.begin() + min(count, result.size()),
                 result.end(),
                 [](const ProcessInfo &a, const ProcessInfo &b)
                 {
                     return a.name < b.name;
                 });

    if (result.size() > count)
        result.resize(count);

    return result;
}

vector<ProcessInfo> ProcessCollector::getTopProcesses(int sort_by, size_t count) const
{
    if (sort_by == 0)
        return getTopProcessesByCpu(count);
    else if (sort_by == 1)
        return getTopProcessesByMemory(count);
    else if (sort_by == 2)
        return getTopProcessesByPid(count);
    else if (sort_by == 3)
        return getTopProcessesByName(count);
    return processes;
}