/**
 * @file cpu_collector.cpp
 * @brief CPU 정보 수집 모듈 구현 파일
 * @author System Monitor Team
 */

#include "collectors/cpu_collector.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <sensors/sensors.h>
#include <time.h>
#include <sys/utsname.h>
using namespace std;

namespace
{
    /**
     * @brief 센서 초기화 상태를 나타내는 변수
     * @details libsensors 라이브러리의 초기화 상태를 추적합니다.
     */
    bool sensors_initialized = false;
}

/**
 * @brief 온도 데이터 캐싱을 위한 시간 제한 상수(밀리초)
 * @details CPU 온도 정보 갱신 주기를 제한하기 위한 값입니다.
 */
static constexpr unsigned long CACHE_TIMEOUT_MS = 1000; // 1초

/**
 * @brief CPUCollector 클래스 생성자
 * @details CPU 정보 관련 필드를 초기화하고 기본 CPU 정보를 수집합니다.
 */
CPUCollector::CPUCollector()
{
    cpuInfo.model = "";
    cpuInfo.architecture = "";
    cpuInfo.usage = 0.0f;
    cpuInfo.temperature = 0.0f;
    cpuInfo.total_cores = 0;
    cpuInfo.total_logical_cores = 0;
    cpuInfo.is_hyperthreading = false;
    // 추가 필드 초기화
    cpuInfo.has_vmx = false;
    cpuInfo.has_svm = false;
    cpuInfo.has_avx = false;
    cpuInfo.has_avx2 = false;
    cpuInfo.has_neon = false;
    cpuInfo.has_sve = false;
    cpuInfo.cache_size = 0;
    cpuInfo.clock_speed = 0.0f;
    cpuInfo.vendor = "";

    // 캐시 시간 초기화
    last_temp_read_time = 0;

    collectCpuInfo();
    collectCacheInfo();
    getBaseCpuSpeed();
}

/**
 * @brief CPU 하드웨어 정보를 수집하는 메서드
 * @details /proc/cpuinfo 파일과 시스템 호출을 통해 CPU 모델, 아키텍처,
 *          코어 수, 하이퍼스레딩 상태, 지원 기능 등의 정보를 수집합니다.
 */
void CPUCollector::collectCpuInfo()
{
    // 먼저 uname을 사용하여 커널이 인식하는 현재 아키텍처 확인
    struct utsname sysInfo;
    if (uname(&sysInfo) == 0)
    {
        string machineName = sysInfo.machine;
        cpuInfo.architecture = machineName; // 기본값으로 uname의 결과 사용
    }

    // /proc/cpuinfo 파일 열기
    char cpuinfo_line[1024]; // 플래그 정보는 길 수 있어 크기 증가
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL)
    {
        return;
    }

    if (fp != NULL)
    {
        int siblings = 0, cores = 0;

        while (fgets(cpuinfo_line, sizeof(cpuinfo_line), fp))
        {
            if (strstr(cpuinfo_line, "siblings"))
            {
                sscanf(cpuinfo_line, "siblings\t: %d", &siblings);
            }
            else if (strstr(cpuinfo_line, "cpu cores"))
            {
                sscanf(cpuinfo_line, "cpu cores\t: %d", &cores);
            }
            else if (strstr(cpuinfo_line, "CPU implementer"))
            {
                unsigned int implementer;
                if (sscanf(cpuinfo_line, "CPU implementer\t: %x", &implementer) == 1)
                {
                    // ARM 제조사 코드
                    switch (implementer)
                    {
                    case 0x41:
                        cpuInfo.vendor = "ARM";
                        break;
                    case 0x42:
                        cpuInfo.vendor = "Broadcom";
                        break;
                    case 0x43:
                        cpuInfo.vendor = "Cavium";
                        break;
                    case 0x44:
                        cpuInfo.vendor = "DEC";
                        break;
                    case 0x4e:
                        cpuInfo.vendor = "Nvidia";
                        break;
                    case 0x51:
                        cpuInfo.vendor = "Qualcomm";
                        break;
                    case 0x53:
                        cpuInfo.vendor = "Samsung";
                        break;
                    case 0x56:
                        cpuInfo.vendor = "Marvell";
                        break;
                    case 0x69:
                        cpuInfo.vendor = "Intel";
                        break;
                    default:
                        cpuInfo.vendor = "Unknown ARM vendor";
                        break;
                    }
                }
            }

            if (strstr(cpuinfo_line, "model name") || strstr(cpuinfo_line, "Processor"))
            {
                char model[256];
                if (strstr(cpuinfo_line, "model name"))
                    sscanf(cpuinfo_line, "model name\t: %[^\n]", model);
                else
                    sscanf(cpuinfo_line, "Processor\t: %[^\n]", model);

                cpuInfo.model = model;
            }

            // RISC-V 확인
            if (strstr(cpuinfo_line, "hart"))
            {
                cpuInfo.vendor = "RISC-V";
            }

            // MIPS 정보 확인
            if (strstr(cpuinfo_line, "cpu model"))
            {
                char model[256];
                sscanf(cpuinfo_line, "cpu model\t: %[^\n]", model);
                cpuInfo.model = model;
            }

            // 플래그 확인 (x86 계열)
            if (strstr(cpuinfo_line, "flags"))
            {
                char flags[1024];
                sscanf(cpuinfo_line, "flags\t: %[^\n]", flags);
                string flagsStr(flags);

                cpuInfo.has_vmx = (flagsStr.find("vmx") != string::npos);
                cpuInfo.has_svm = (flagsStr.find("svm") != string::npos);
                cpuInfo.has_avx = (flagsStr.find("avx") != string::npos);
                cpuInfo.has_avx2 = (flagsStr.find("avx2") != string::npos);
            }

            // ARM 기능 확인
            if (strstr(cpuinfo_line, "Features"))
            {
                char features[1024];
                sscanf(cpuinfo_line, "Features\t: %[^\n]", features);
                string featuresStr(features);

                cpuInfo.has_neon = (featuresStr.find("neon") != string::npos);
                cpuInfo.has_sve = (featuresStr.find("sve") != string::npos);
            }

            if (strstr(cpuinfo_line, "cache size"))
            {
                int cache;
                sscanf(cpuinfo_line, "cache size\t: %d", &cache);
                cpuInfo.cache_size = cache;
            }
            else if (strstr(cpuinfo_line, "cpu MHz"))
            {
                float mhz;
                sscanf(cpuinfo_line, "cpu MHz\t\t: %f", &mhz);
                cpuInfo.clock_speed = mhz;
            }
        }

        // 하이퍼스레딩 확인
        if (siblings > 0 && cores > 0)
        {
            if (siblings > cores)
            {
                cpuInfo.is_hyperthreading = true;
                cpuInfo.total_cores = static_cast<size_t>(cores);
                cpuInfo.total_logical_cores = static_cast<size_t>(siblings);
            }
            else
            {
                cpuInfo.is_hyperthreading = false;
                cpuInfo.total_cores = static_cast<size_t>(cores);
                cpuInfo.total_logical_cores = static_cast<size_t>(cores);
            }
        }

        fclose(fp);
    }
}

/**
 * @brief CPU 사용량과 온도 정보를 수집하는 메서드
 * @details /proc/stat 파일을 분석하여 CPU 및 코어별 사용률을 계산하고,
 *          libsensors를 통해 CPU 온도를 측정합니다.
 *          지정된 캐시 주기에 따라 온도 정보를 갱신합니다.
 * @see Collector::collect()
 */
void CPUCollector::collect()
{
    // sensors 초기화 (처음 한 번만)
    if (!sensors_initialized)
    {
        if (sensors_init(NULL) == 0)
        {
            sensors_initialized = true;
        }
    }

    FILE *pStat = NULL;
    char line[256];

    pStat = fopen("/proc/stat", "r");
    if (pStat == NULL)
    {
        // 파일 열기 실패 시 이전 값 유지
        return;
    }

    // 전체 CPU 사용량 읽기
    if (fgets(line, sizeof(line), pStat))
    {
        sscanf(line, "cpu %ld %ld %ld %ld", &curJiffies.user,
               &curJiffies.nice, &curJiffies.system, &curJiffies.idle);
    }

    // 각 코어별 사용량 읽기
    vector<stJiffies> newCoreJiffies;
    newCoreJiffies.reserve(prevCoreJiffies.size()); // 메모리 재할당 방지
    size_t core_id = 0;                             // 코어 ID 추적

    while (fgets(line, sizeof(line), pStat))
    {
        stJiffies coreJiffies;
        if (sscanf(line, "cpu%*d %ld %ld %ld %ld",
                   &coreJiffies.user, &coreJiffies.nice,
                   &coreJiffies.system, &coreJiffies.idle) == 4)
        {
            newCoreJiffies.push_back(coreJiffies);

            // cores 벡터 크기 조정 (필요시)
            if (core_id >= cpuInfo.cores.size())
            {
                CpuCoreInfo newCore;
                newCore.id = core_id;
                newCore.usage = 0.0f;
                newCore.temperature = 0.0f;
                cpuInfo.cores.push_back(newCore);
            }
            else
            {
                // ID 값 설정
                cpuInfo.cores[static_cast<size_t>(core_id)].id = static_cast<size_t>(core_id);
            }
            core_id++;
        }
        else
        {
            break; // cpu 정보가 아닌 줄을 만나면 중단
        }
    }

    // 전체 CPU 사용률 계산
    // 초기 상태가 아닐 때만 CPU 사용률 계산
    if (prevJiffies.user != 0 || prevJiffies.nice != 0 ||
        prevJiffies.system != 0 || prevJiffies.idle != 0)
    {

        struct stJiffies diffJiffies;
        diffJiffies.user = curJiffies.user - prevJiffies.user;
        diffJiffies.nice = curJiffies.nice - prevJiffies.nice;
        diffJiffies.system = curJiffies.system - prevJiffies.system;
        diffJiffies.idle = curJiffies.idle - prevJiffies.idle;

        long int totalJiffies = diffJiffies.user + diffJiffies.nice +
                                diffJiffies.system + diffJiffies.idle;

        if (totalJiffies > 0)
        {
            double idle_ratio = static_cast<double>(diffJiffies.idle) / static_cast<double>(totalJiffies);
            cpuInfo.usage = static_cast<float>(100.0 * (1.0 - idle_ratio));
        }
    }

    // 각 코어별 사용률 계산
    cpuInfo.cores.resize(newCoreJiffies.size());
    if (!prevCoreJiffies.empty())
    {
        for (size_t i = 0; i < newCoreJiffies.size(); ++i)
        {
            stJiffies diff;
            diff.user = newCoreJiffies[i].user - prevCoreJiffies[i].user;
            diff.nice = newCoreJiffies[i].nice - prevCoreJiffies[i].nice;
            diff.system = newCoreJiffies[i].system - prevCoreJiffies[i].system;
            diff.idle = newCoreJiffies[i].idle - prevCoreJiffies[i].idle;

            long int total = diff.user + diff.nice + diff.system + diff.idle;
            if (total > 0)
            {
                double idle_ratio = static_cast<double>(diff.idle) / static_cast<double>(total);
                cpuInfo.cores[i].usage = static_cast<float>(100.0 * (1.0 - idle_ratio));
            }
        }
    }

    prevCoreJiffies = newCoreJiffies;
    prevJiffies = curJiffies;
    fclose(pStat);

    // 온도 데이터 캐싱 구현
    unsigned long current_time = static_cast<unsigned long>(time(NULL) * 1000);

    // sensors가 초기화되지 않았거나 캐시 시간이 지났을 때만 온도 데이터 갱신
    if (sensors_initialized && (current_time - last_temp_read_time > CACHE_TIMEOUT_MS))
    {
        last_temp_read_time = current_time;

        const sensors_chip_name *chip;
        const sensors_feature *feature;
        const sensors_subfeature *subfeature;
        int chip_nr = 0;

        // 모든 센서 칩을 순회
        while ((chip = sensors_get_detected_chips(NULL, &chip_nr)))
        {
            // coretemp 드라이버를 사용하는 칩만 처리
            if (strcmp(chip->prefix, "coretemp") == 0)
            {
                int feature_nr = 0;
                while ((feature = sensors_get_features(chip, &feature_nr)))
                {
                    // SENSORS_FEATURE_TEMP 타입의 피처만 처리
                    if (feature->type == SENSORS_FEATURE_TEMP)
                    {
                        subfeature = sensors_get_subfeature(chip, feature,
                                                            SENSORS_SUBFEATURE_TEMP_INPUT);
                        if (subfeature)
                        {
                            double temp;
                            if (sensors_get_value(chip, subfeature->number, &temp) == 0)
                            {
                                // 온도가 유효한 범위인지 확인 (0°C ~ 100°C)
                                if (temp >= 0.0 && temp <= 100.0)
                                {
                                    // 센서 레이블에서 코어 번호 추출
                                    const char *label = sensors_get_label(chip, feature);
                                    int core_num = -1;
                                    if (label && sscanf(label, "Core %d", &core_num) == 1)
                                    {
                                        if (core_num >= 0 && static_cast<size_t>(core_num) < cpuInfo.cores.size())
                                        {
                                            cpuInfo.cores[static_cast<size_t>(core_num)].temperature = static_cast<float>(temp);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 하이퍼스레딩인 경우 논리적 코어의 온도를 물리적 코어와 동일하게 설정
        if (cpuInfo.is_hyperthreading)
        {
            for (size_t i = cpuInfo.total_cores; i < cpuInfo.cores.size(); i++)
            {
                cpuInfo.cores[i].temperature = cpuInfo.cores[i - cpuInfo.total_cores].temperature;
            }
        }

        // 전체 CPU 온도는 코어 온도의 평균으로 계산
        if (!cpuInfo.cores.empty())
        {
            double total = 0.0;
            for (const CpuCoreInfo &temp : cpuInfo.cores) // 참조로 변경하여 복사 방지
            {
                total += temp.temperature;
            }
            cpuInfo.temperature = static_cast<float>(total / static_cast<double>(cpuInfo.cores.size()));
        }
    }
}

/**
 * @brief 수집된 CPU 정보를 반환하는 메서드
 * @return CPU 정보 구조체 (CpuInfo)
 */
const CpuInfo CPUCollector::getCpuInfo() const
{
    return cpuInfo;
}

/**
 * @brief CPUCollector 클래스 소멸자
 * @details libsensors 라이브러리 자원을 정리합니다.
 */
CPUCollector::~CPUCollector()
{
    if (sensors_initialized)
    {
        sensors_cleanup();
        sensors_initialized = false;
    }
}

void CPUCollector::collectCacheInfo()
{
    // L1, L2, L3 캐시 크기를 저장할 변수
    cpuInfo.l1_cache_size = 0;
    cpuInfo.l2_cache_size = 0;
    cpuInfo.l3_cache_size = 0;

    // /sys/devices/system/cpu/cpu0/cache/ 디렉토리에서 캐시 정보 읽기
    for (int i = 0; i < 4; i++)
    { // 일반적으로 인덱스 0-3까지 확인
        string path = "/sys/devices/system/cpu/cpu0/cache/index" + to_string(i);

        // 캐시 레벨 확인
        FILE *level_file = fopen((path + "/level").c_str(), "r");
        if (level_file)
        {
            int level;
            fscanf(level_file, "%d", &level);
            fclose(level_file);

            // 캐시 크기 읽기
            FILE *size_file = fopen((path + "/size").c_str(), "r");
            if (size_file)
            {
                int size;
                char unit[3];
                fscanf(size_file, "%d%2s", &size, unit);
                fclose(size_file);

                // KB 단위로 변환
                int size_kb = size;
                if (strcmp(unit, "M") == 0)
                {
                    size_kb *= 1024;
                }

                // 레벨에 따라 저장
                if (level == 1)
                {
                    cpuInfo.l1_cache_size = size_kb;
                }
                else if (level == 2)
                {
                    cpuInfo.l2_cache_size = size_kb;
                }
                else if (level == 3)
                {
                    cpuInfo.l3_cache_size = size_kb;
                }
            }
        }
    }
}

void CPUCollector::getBaseCpuSpeed()
{
    // 기존 코드 유지 (min, max MHz 수집)
    FILE *fp = popen("lscpu | grep 'CPU max MHz\\|CPU min MHz'", "r");
    if (fp != NULL)
    {
        char line[256];
        while (fgets(line, sizeof(line), fp))
        {
            if (strstr(line, "CPU max MHz"))
            {
                float mhz;
                if (sscanf(line, "CPU max MHz: %f", &mhz) == 1)
                {
                    cpuInfo.max_clock_speed = mhz;
                }
            }
            else if (strstr(line, "CPU min MHz"))
            {
                float mhz;
                if (sscanf(line, "CPU min MHz: %f", &mhz) == 1)
                {
                    cpuInfo.min_clock_speed = mhz;
                }
            }
        }
        pclose(fp);
    }

    // 기본 클럭 속도 수집 (dmidecode 사용)
    FILE *base_fp = popen("sudo dmidecode -t processor | grep 'Current Speed'", "r");
    if (base_fp != NULL)
    {
        char line[256];
        if (fgets(line, sizeof(line), base_fp))
        {
            float ghz;
            if (sscanf(line, "Current Speed: %f GHz", &ghz) == 1)
            {
                cpuInfo.base_clock_speed = ghz * 1000; // GHz를 MHz로 변환
            }
        }
        pclose(base_fp);
    }

    // 백업 방법: base_clock_speed가 설정되지 않았다면 /proc/cpuinfo의 모델 이름에서 추출 시도
    if (cpuInfo.base_clock_speed == 0.0f)
    {
        FILE *proc_fp = fopen("/proc/cpuinfo", "r");
        if (proc_fp != NULL)
        {
            char line[1024];
            while (fgets(line, sizeof(line), proc_fp))
            {
                if (strstr(line, "model name"))
                {
                    // 모델 이름에서 GHz 추출 시도 (예: "Intel(R) Core(TM) i7-9700K @ 3.60GHz")
                    float ghz;
                    if (strstr(line, "GHz") && sscanf(strstr(line, "@"), "@ %f", &ghz) == 1)
                    {
                        cpuInfo.base_clock_speed = ghz * 1000; // GHz를 MHz로 변환
                        break;
                    }
                }
            }
            fclose(proc_fp);
        }
    }
}