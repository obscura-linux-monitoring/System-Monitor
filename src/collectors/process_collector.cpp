/**
 * @file process_collector.cpp
 * @brief 시스템에서 실행 중인 프로세스 정보를 수집하고 관리하는 클래스 구현
 *
 * ProcessCollector 클래스는 시스템에서 실행 중인 모든 프로세스의 상세 정보를 수집하고,
 * 다양한 기준(CPU 사용량, 메모리 사용량, PID, 이름)으로 정렬하여 제공합니다.
 * 또한 특정 프로세스를 종료하는 기능도 포함하고 있습니다.
 */
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

/**
 * @brief 프로세스 상태 코드를 사람이 읽기 쉬운 텍스트로 변환합니다.
 *
 * @param status 프로세스 상태 코드 (예: "R", "S", "I" 등)
 * @return string 사람이 읽기 쉬운 상태 설명
 */
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

/**
 * @brief 지정된 비교 함수를 사용하여 정렬된 프로세스 목록을 반환합니다.
 *
 * @tparam Comparator 프로세스를 비교하는 함수 객체 타입
 * @param comp 프로세스 정렬에 사용할 비교 함수 객체
 * @return vector<ProcessInfo> 정렬된 프로세스 정보 벡터
 */
template <typename Comparator>
vector<ProcessInfo> ProcessCollector::getSortedProcesses(Comparator comp) const
{
    vector<ProcessInfo> result = processes;
    sort(result.begin(), result.end(), comp);
    return result;
}

/**
 * @brief 시스템의 모든 실행 중인 프로세스 정보를 수집합니다.
 *
 * 이 함수는 시스템에서 실행 중인 모든 프로세스의 상세 정보를 수집하여
 * 내부 processes 벡터에 저장합니다. 수집되는 정보는 PID, CPU/메모리 사용량,
 * 시작 시간, 상태, 명령어 등을 포함합니다.
 *
 * @throw runtime_error 프로세스 정보에 접근할 수 없는 경우 발생
 */
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
        unsigned long clk_tck = static_cast<unsigned long>(sysconf(_SC_CLK_TCK));
        process.start_time = boot_time + static_cast<time_t>(proc_info.start_time / clk_tck);

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
        process.cpu_time = static_cast<float>((proc_info.utime + proc_info.stime)) / static_cast<float>(sysconf(_SC_CLK_TCK));

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

/**
 * @brief CPU 사용량을 기준으로 정렬된 모든 프로세스 목록을 반환합니다.
 *
 * @return vector<ProcessInfo> CPU 사용량이 높은 순으로 정렬된 프로세스 정보 벡터
 */
vector<ProcessInfo> ProcessCollector::getProcessesByCpuUsage() const
{
    return getSortedProcesses([](const ProcessInfo &a, const ProcessInfo &b)
                              { return a.cpu_usage > b.cpu_usage; });
}

/**
 * @brief 메모리 사용량을 기준으로 정렬된 모든 프로세스 목록을 반환합니다.
 *
 * @return vector<ProcessInfo> 메모리 사용량이 높은 순으로 정렬된 프로세스 정보 벡터
 */
vector<ProcessInfo> ProcessCollector::getProcessesByMemoryUsage() const
{
    return getSortedProcesses([](const ProcessInfo &a, const ProcessInfo &b)
                              { return a.memory_rss > b.memory_rss; });
}

/**
 * @brief PID를 기준으로 정렬된 모든 프로세스 목록을 반환합니다.
 *
 * @return vector<ProcessInfo> PID가 낮은 순으로 정렬된 프로세스 정보 벡터
 */
vector<ProcessInfo> ProcessCollector::getProcessesByPid() const
{
    return getSortedProcesses([](const ProcessInfo &a, const ProcessInfo &b)
                              { return a.pid < b.pid; });
}

/**
 * @brief 프로세스 이름을 기준으로 정렬된 모든 프로세스 목록을 반환합니다.
 *
 * @return vector<ProcessInfo> 이름 알파벳 순으로 정렬된 프로세스 정보 벡터
 */
vector<ProcessInfo> ProcessCollector::getProcessesByName() const
{
    return getSortedProcesses([](const ProcessInfo &a, const ProcessInfo &b)
                              { return a.name < b.name; });
}

/**
 * @brief 지정된 정렬 기준에 따라 정렬된 모든 프로세스 목록을 반환합니다.
 *
 * @param sort_by 정렬 기준 (0: CPU 사용량, 1: 메모리 사용량, 2: PID, 3: 이름)
 * @return vector<ProcessInfo> 지정된 기준으로 정렬된 프로세스 정보 벡터
 */
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

/**
 * @brief 지정된 PID의 프로세스에 종료 신호(SIGTERM)를 보냅니다.
 *
 * @param pid 종료할 프로세스의 ID
 * @return bool 성공 여부 (true: 성공, false: 실패)
 */
bool ProcessCollector::killProcess(pid_t pid)
{
    return (kill(pid, SIGTERM) == 0);
}

/**
 * @brief 메모리 사용량이 높은 상위 N개 프로세스 목록을 반환합니다.
 *
 * 전체 목록을 정렬하는 대신 부분 정렬을 사용하여 성능을 향상시킵니다.
 *
 * @param count 반환할 프로세스 수
 * @return vector<ProcessInfo> 메모리 사용량 기준 상위 N개 프로세스
 */
vector<ProcessInfo> ProcessCollector::getTopProcessesByMemory(size_t count) const
{
    vector<ProcessInfo> result = processes;

    // 전체 정렬 대신 부분 정렬 사용 (성능 향상)
    partial_sort(result.begin(),
                 result.begin() + static_cast<ptrdiff_t>(min(count, result.size())),
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

/**
 * @brief CPU 사용량이 높은 상위 N개 프로세스 목록을 반환합니다.
 *
 * 전체 목록을 정렬하는 대신 부분 정렬을 사용하여 성능을 향상시킵니다.
 *
 * @param count 반환할 프로세스 수
 * @return vector<ProcessInfo> CPU 사용량 기준 상위 N개 프로세스
 */
vector<ProcessInfo> ProcessCollector::getTopProcessesByCpu(size_t count) const
{
    vector<ProcessInfo> result = processes;

    partial_sort(result.begin(),
                 result.begin() + static_cast<ptrdiff_t>(min(count, result.size())),
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

/**
 * @brief PID가 낮은 순으로 상위 N개 프로세스 목록을 반환합니다.
 *
 * 전체 목록을 정렬하는 대신 부분 정렬을 사용하여 성능을 향상시킵니다.
 *
 * @param count 반환할 프로세스 수
 * @return vector<ProcessInfo> PID 기준 상위 N개 프로세스
 */
vector<ProcessInfo> ProcessCollector::getTopProcessesByPid(size_t count) const
{
    vector<ProcessInfo> result = processes;
    partial_sort(result.begin(),
                 result.begin() + static_cast<ptrdiff_t>(min(count, result.size())),
                 result.end(),
                 [](const ProcessInfo &a, const ProcessInfo &b)
                 {
                     return a.pid < b.pid;
                 });

    if (result.size() > count)
        result.resize(count);

    return result;
}

/**
 * @brief 이름 알파벳 순으로 상위 N개 프로세스 목록을 반환합니다.
 *
 * 전체 목록을 정렬하는 대신 부분 정렬을 사용하여 성능을 향상시킵니다.
 *
 * @param count 반환할 프로세스 수
 * @return vector<ProcessInfo> 이름 기준 상위 N개 프로세스
 */
vector<ProcessInfo> ProcessCollector::getTopProcessesByName(size_t count) const
{
    vector<ProcessInfo> result = processes;
    partial_sort(result.begin(),
                 result.begin() + static_cast<ptrdiff_t>(min(count, result.size())),
                 result.end(),
                 [](const ProcessInfo &a, const ProcessInfo &b)
                 {
                     return a.name < b.name;
                 });

    if (result.size() > count)
        result.resize(count);

    return result;
}

/**
 * @brief 지정된 정렬 기준에 따라 상위 N개 프로세스 목록을 반환합니다.
 *
 * @param sort_by 정렬 기준 (0: CPU 사용량, 1: 메모리 사용량, 2: PID, 3: 이름)
 * @param count 반환할 프로세스 수
 * @return vector<ProcessInfo> 지정된 기준으로 정렬된 상위 N개 프로세스
 */
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