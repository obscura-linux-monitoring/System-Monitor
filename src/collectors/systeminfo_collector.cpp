#include "collectors/systeminfo_collector.h"
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <cstring>
#include <cstdio>

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
 * @brief 시스템 정보를 수집하는 메소드
 * 
 * 다음과 같은 시스템 정보를 수집합니다:
 * - 호스트 이름
 * - 운영 체제 정보(이름, 버전, 커널 버전, 아키텍처)
 * - 시스템 부팅 시간 및 가동 시간
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
}