#include "collectors/systeminfo_collector.h"
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <sstream>

using namespace std;

/**
 * @brief SystemInfoCollector 클래스의 생성자
 *
 * SystemInfo 구조체를 초기화합니다.
 */
SystemInfoCollector::SystemInfoCollector()
{
    system_info = SystemInfo{};
}

/**
 * @brief SystemInfoCollector 클래스의 소멸자
 */
SystemInfoCollector::~SystemInfoCollector()
{
}

/**
 * @brief 수집된 시스템 정보를 반환합니다.
 *
 * @return 시스템 정보가 담긴 SystemInfo 구조체의 상수 참조
 */
const SystemInfo &SystemInfoCollector::getSystemInfo() const
{
    return system_info;
}

/**
 * @brief 시스템의 총 프로세스 수를 계산합니다.
 *
 * @return 시스템에서 실행 중인 총 프로세스 수
 */
unsigned int SystemInfoCollector::countProcesses()
{
    unsigned int count = 0;
    DIR *proc_dir = opendir("/proc");

    if (proc_dir != nullptr)
    {
        struct dirent *entry;
        while ((entry = readdir(proc_dir)) != nullptr)
        {
            // /proc 디렉토리에서 숫자로 된 디렉토리는 프로세스 ID를 의미합니다
            if (entry->d_type == DT_DIR)
            {
                bool is_numeric = true;
                for (const char *p = entry->d_name; *p; ++p)
                {
                    if (*p < '0' || *p > '9')
                    {
                        is_numeric = false;
                        break;
                    }
                }
                if (is_numeric)
                {
                    count++;
                }
            }
        }
        closedir(proc_dir);
    }

    return count;
}

/**
 * @brief 시스템의 총 스레드 수를 계산합니다.
 *
 * @return 시스템에서 실행 중인 총 스레드 수
 */
unsigned int SystemInfoCollector::countThreads()
{
    unsigned int total_threads = 0;
    DIR *proc_dir = opendir("/proc");

    if (proc_dir != nullptr)
    {
        struct dirent *entry;
        while ((entry = readdir(proc_dir)) != nullptr)
        {
            // /proc 디렉토리에서 숫자로 된 디렉토리는 프로세스 ID를 의미합니다
            if (entry->d_type == DT_DIR)
            {
                bool is_numeric = true;
                for (const char *p = entry->d_name; *p; ++p)
                {
                    if (*p < '0' || *p > '9')
                    {
                        is_numeric = false;
                        break;
                    }
                }

                if (is_numeric)
                {
                    // 해당 프로세스의 스레드 정보를 읽습니다
                    string status_path = string("/proc/") + entry->d_name + "/status";
                    ifstream status_file(status_path);

                    if (status_file.is_open())
                    {
                        string line;
                        while (getline(status_file, line))
                        {
                            if (line.compare(0, 8, "Threads:") == 0)
                            {
                                istringstream iss(line.substr(8));
                                unsigned int threads;
                                if (iss >> threads)
                                {
                                    total_threads += threads;
                                }
                                break;
                            }
                        }
                        status_file.close();
                    }
                }
            }
        }
        closedir(proc_dir);
    }

    return total_threads;
}

/**
 * @brief 시스템의 총 열린 파일 디스크립터 수를 계산합니다.
 *
 * @return 시스템에서 열린 총 파일 디스크립터 수
 */
unsigned int SystemInfoCollector::countFileDescriptors()
{
    unsigned int total_fds = 0;

    // /proc/sys/fs/file-nr에서 파일 디스크립터 정보를 읽습니다
    ifstream file_nr("/proc/sys/fs/file-nr");
    if (file_nr.is_open())
    {
        unsigned int allocated, unused, max;
        file_nr >> allocated >> unused >> max;
        total_fds = allocated;
        file_nr.close();
    }
    else
    {
        // 대체 방법: 각 프로세스별 열린 파일 디스크립터 수 계산
        DIR *proc_dir = opendir("/proc");
        if (proc_dir != nullptr)
        {
            struct dirent *entry;
            while ((entry = readdir(proc_dir)) != nullptr)
            {
                if (entry->d_type == DT_DIR)
                {
                    bool is_numeric = true;
                    for (const char *p = entry->d_name; *p; ++p)
                    {
                        if (*p < '0' || *p > '9')
                        {
                            is_numeric = false;
                            break;
                        }
                    }

                    if (is_numeric)
                    {
                        // 해당 프로세스의 fd 디렉토리에서 열린 파일 수 계산
                        string fd_path = string("/proc/") + entry->d_name + "/fd";
                        DIR *fd_dir = opendir(fd_path.c_str());
                        if (fd_dir != nullptr)
                        {
                            struct dirent *fd_entry;
                            while ((fd_entry = readdir(fd_dir)) != nullptr)
                            {
                                if (fd_entry->d_name[0] != '.')
                                {
                                    total_fds++;
                                }
                            }
                            closedir(fd_dir);
                        }
                    }
                }
            }
            closedir(proc_dir);
        }
    }

    return total_fds;
}

/**
 * @brief 시스템 정보를 수집하는 메소드
 *
 * 다음과 같은 시스템 정보를 수집합니다:
 * - 호스트 이름
 * - 운영 체제 정보(이름, 버전, 커널 버전, 아키텍처)
 * - 시스템 부팅 시간 및 가동 시간
 * - 총 프로세스 수, 스레드 수, 파일 디스크립터 수
 */
void SystemInfoCollector::collect()
{
    // 호스트 이름 수집
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        // 안전한 문자열 처리를 위한 null 종료 문자 보장
        hostname[sizeof(hostname) - 1] = '\0';
        system_info.hostname = hostname;
    }
    else
    {
        system_info.hostname = "Unknown";
    }

    // 운영 체제 정보 수집
    struct utsname system_data;
    if (uname(&system_data) == 0)
    {
        system_info.os_name = system_data.sysname;
        system_info.os_version = system_data.release;
        system_info.os_kernel_version = system_data.version;
        system_info.os_architecture = system_data.machine;
    }
    else
    {
        system_info.os_name = "Unknown";
        system_info.os_version = "Unknown";
        system_info.os_kernel_version = "Unknown";
        system_info.os_architecture = "Unknown";
    }

    // 부팅 시간 수집
    struct sysinfo si;
    if (sysinfo(&si) == 0)
    {
        system_info.uptime = si.uptime;
        system_info.boot_time = time(nullptr) - si.uptime;
    }
    else
    {
        system_info.uptime = 0;
        system_info.boot_time = 0;
    }

    // 추가된 정보 수집: 프로세스 수, 스레드 수, 파일 디스크립터 수
    system_info.total_processes = countProcesses();
    system_info.total_threads = countThreads();
    system_info.total_file_descriptors = countFileDescriptors();
}