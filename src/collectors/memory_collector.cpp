/**
 * @file memory_collector.cpp
 * @brief 시스템 메모리 정보를 수집하는 MemoryCollector 클래스의 구현
 */
#include "collectors/memory_collector.h"
#include <fstream>
#include <sstream>
#include <string>
#include "utils/command_run.h"
#include "log/logger.h"

using namespace std;

/**
 * @brief 메모리 정보 구조체를 초기화합니다.
 *
 * 모든 메모리 관련 필드를 0으로 설정합니다.
 */
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
    memoryInfo.data_rate = 0;
    memoryInfo.total_slot_count = 0;
    memoryInfo.using_slot_count = 0;
    memoryInfo.form_factor = "";
    memoryInfo.paged_pool_size = 0;
    memoryInfo.non_paged_pool_size = 0;

    // 메모리 속도 (MT/s) - dmidecode 명령어 사용
    try
    {
        string dmidecode_output = exec("sudo dmidecode -t memory 2>/dev/null | grep 'Speed'");
        istringstream dmidecode_stream(dmidecode_output);
        string _line;

        int date_rate_count = 0, data_rate_sum = 0;
        while (getline(dmidecode_stream, _line))
        {
            if (_line.find("MT/s") != string::npos || _line.find("MHz") != string::npos)
            {
                size_t pos = _line.find(':');
                if (pos != string::npos)
                {
                    string speed_str = _line.substr(pos + 1);
                    // 숫자만 추출
                    string digits;
                    for (char c : speed_str)
                    {
                        if (isdigit(c))
                        {
                            digits += c;
                        }
                    }

                    if (!digits.empty())
                    {
                        data_rate_sum += stoull(digits);
                        date_rate_count++;
                    }
                }
            }
        }

        if (date_rate_count > 0)
        {
            memoryInfo.data_rate = data_rate_sum / date_rate_count;
        }
        else
        {
            memoryInfo.data_rate = 0;
        }
    }
    catch (const runtime_error &e)
    {
        LOG_ERROR("Failed to get memory data rate: %s", e.what());
    }

    // 메모리 슬롯 정보 - dmidecode로 계산
    try
    {
        string memory_devices = exec("sudo dmidecode -t memory | grep -c 'Memory Device'");
        if (!memory_devices.empty())
        {
            memoryInfo.total_slot_count = (uint16_t)stoi(memory_devices);
        }
    }
    catch (const runtime_error &e)
    {
        LOG_ERROR("Failed to get memory total slot count: %s", e.what());
    }

    try
    {
        string used_slots = exec("sudo dmidecode -t memory | grep -A 16 'Memory Device' | grep -c 'Size: [0-9]'");
        if (!used_slots.empty())
        {
            memoryInfo.using_slot_count = (uint16_t)stoi(used_slots);
        }
    }
    catch (const runtime_error &e)
    {
        LOG_ERROR("Failed to get memory using slot count: %s", e.what());
    }

    // 메모리 form factor 추출
    memoryInfo.form_factor = "Etc";
    try
    {
        string form_factor = "Etc";
        string cmd_output = exec("sudo dmidecode -t memory | grep -i 'Form Factor:' | head -1");

        if (!cmd_output.empty())
        {
            size_t pos = cmd_output.find(':');
            if (pos != string::npos)
            {
                form_factor = cmd_output.substr(pos + 1);
                // 앞뒤 공백 제거
                form_factor.erase(0, form_factor.find_first_not_of(" \t\n\r"));
                form_factor.erase(form_factor.find_last_not_of(" \t\n\r") + 1);
            }
        }

        memoryInfo.form_factor = form_factor;
    }
    catch (const runtime_error &e)
    {
        LOG_ERROR("Failed to get memory form factor: %s", e.what());
    }
}

/**
 * @brief 시스템의 메모리 정보를 수집합니다.
 *
 * /proc/meminfo 파일에서 메모리 정보를 읽어와 MemoryInfo 구조체에 저장합니다.
 * 메모리 사용량, 총 메모리, 사용 가능한 메모리 등의 정보를 수집하고 계산합니다.
 *
 * @throw runtime_error /proc/meminfo 파일을 열 수 없을 경우 예외를 발생시킵니다.
 */
void MemoryCollector::collect()
{
    ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open())
    {
        throw runtime_error("Cannot open /proc/meminfo");
    }

    // 메모리 정보 초기화
    clear();

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
        memoryInfo.usage_percent = (float)memoryInfo.used / (float)memoryInfo.total * 100.0f;
    }
    else
    {
        memoryInfo.usage_percent = 0.0f;
    }
}

/**
 * @brief 수집된 메모리 정보를 반환합니다.
 *
 * @return MemoryInfo 수집된 시스템 메모리 정보를 담고 있는 구조체
 */
MemoryInfo MemoryCollector::getMemoryInfo() const
{
    return memoryInfo;
}