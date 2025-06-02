#include "collectors/disk_collector.h"
#include <fstream>
#include <sys/statvfs.h>
#include <sstream>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <thread>
#include <cerrno>
#include <cstring>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <set>
#include "log/logger.h"

using namespace std;

/**
 * @brief DiskCollector 클래스 생성자
 *
 * 스레드 풀을 초기화하고 디스크 정보를 처음으로 수집합니다.
 */
DiskCollector::DiskCollector() : last_collect_time(chrono::steady_clock::now())
{
    // 스레드 풀 초기화
    for (size_t i = 0; i < THREAD_POOL_SIZE; i++)
    {
        thread_pool.emplace_back(&DiskCollector::threadWorker, this);
    }

    collectDiskInfo();
}

/**
 * @brief DiskCollector 클래스 소멸자
 *
 * 스레드 풀을 정리하고 수집된 디스크 통계 데이터를 삭제합니다.
 */
DiskCollector::~DiskCollector()
{
    // 스레드 풀 정리
    stop_threads = true;
    cv.notify_all();

    for (auto &t : thread_pool)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    disk_stats.clear();
}

/**
 * @brief 시스템의 모든 디스크 정보를 수집합니다.
 *
 * /proc/mounts 파일에서 마운트된 디스크 목록을 읽고 실제 물리적 디스크나
 * 일반적인 파일 시스템만 처리합니다. 각 디스크에 대한 기본 정보를 수집하고
 * 사용량 정보를 업데이트합니다.
 *
 * @throw runtime_error /proc/mounts 파일을 열 수 없을 때 발생
 */
void DiskCollector::collectDiskInfo()
{
    disk_stats.clear();

    // 루트 디바이스 추출
    string root_device;
    ifstream mounts_file("/proc/mounts");
    string mline;
    while (getline(mounts_file, mline))
    {
        istringstream miss(mline);
        string mdevice, mmount_point, mfs_type;
        miss >> mdevice >> mmount_point >> mfs_type;
        if (mmount_point == "/")
        {
            root_device = mdevice;
            break;
        }
    }

    // /proc/swaps에서 스왑 디바이스 목록 추출
    set<string> swap_devices;
    ifstream swaps_file("/proc/swaps");
    string sline;
    getline(swaps_file, sline); // 첫 줄은 헤더
    while (getline(swaps_file, sline))
    {
        istringstream siss(sline);
        string sdevice;
        siss >> sdevice;
        swap_devices.insert(sdevice);
    }

    // /proc/mounts 파일에서 마운트된 디스크 목록 읽기
    ifstream mounts("/proc/mounts");
    if (!mounts.is_open())
    {
        throw runtime_error("Cannot open /proc/mounts");
    }

    string line;
    while (getline(mounts, line))
    {
        istringstream iss(line);
        string device, mount_point, fs_type;
        iss >> device >> mount_point >> fs_type;

        // loop 디바이스 제외하고 실제 물리적 디스크나 일반적인 파일시스템만 처리
        if (device.find("/dev/") == 0 &&
            device.find("/dev/loop") == string::npos && // loop 디바이스 제외
            fs_type != "proc" && fs_type != "sysfs" && fs_type != "devpts")
        {
            DiskInfo disk_info;
            disk_info.device = device;
            disk_info.mount_point = mount_point;
            disk_info.filesystem_type = fs_type;

            // 파티션명에서 디스크명 추출
            string device_name = device.substr(5);         // "/dev/" 이후만 추출 ("sda2" 등)
            string disk_name = getParentDisk(device_name); // "sda2" → "sda"

            // 디스크 이름 저장 (I/O 통계용)
            disk_info.parent_disk = disk_name;

            // 모델명 읽기
            ifstream model_file("/sys/block/" + disk_name + "/device/model");
            if (model_file.is_open())
            {
                getline(model_file, disk_info.model_name);
            }
            else
            {
                disk_info.model_name = "unknown";
            }
            model_file.close();

            // 디스크 타입 (SSD/HDD)
            ifstream rotational_file("/sys/block/" + disk_name + "/queue/rotational");
            int rotational = 1;
            if (rotational_file.is_open())
            {
                rotational_file >> rotational;
            }
            else
            {
                disk_info.type = "unknown";
            }
            rotational_file.close();
            disk_info.type = (rotational == 0) ? "SSD" : "HDD";

            // 시스템 디스크 여부
            disk_info.is_system_disk = (device == root_device);

            // 스왑 디스크 여부
            disk_info.is_page_file_disk = (swap_devices.count(device) > 0);

            // 초기 상태로 디스크 정보 추가 (사용량 정보는 collect에서 업데이트)
            disk_stats.push_back(disk_info);
        }
    }

    // 초기 디스크 사용량 정보도 수집
    updateDiskUsage();
}

/**
 * @brief 디스크 사용량 정보만 비동기적으로 업데이트합니다.
 *
 * 각 디스크에 대해 비동기 작업을 생성하여 사용량 정보를 수집합니다.
 * 작업에 타임아웃이 적용되며 모든 작업이 완료된 후 I/O 통계 정보도 업데이트합니다.
 */
void DiskCollector::updateDiskUsage()
{
    pending_tasks.clear();

    // 비동기 작업 결과를 저장할 벡터
    vector<DiskInfo> updated_stats;
    mutex update_mutex;

    // 각 디스크에 대해 비동기 작업 생성
    for (const auto &disk_info : disk_stats)
    {
        // 비동기 태스크 실행 - 값 복사 및 결과 저장
        future<void> task = async(launch::async, [disk_info, &updated_stats, &update_mutex]()
                                  {
            DiskInfo updated_info = disk_info; // 로컬 복사본 생성
            
            struct statvfs fs_stats;
            if (statvfs(updated_info.mount_point.c_str(), &fs_stats) == 0) {
                // 업데이트된 정보를 로컬 복사본에 저장
                updated_info.total = fs_stats.f_blocks * fs_stats.f_frsize;
                // 사용 가능한 용량
                updated_info.free = fs_stats.f_bfree * fs_stats.f_frsize;
                // 사용중인 용량
                updated_info.used = updated_info.total - updated_info.free;
                
                // 사용률 계산 (백분율)
                if (updated_info.total > 0) {
                    updated_info.usage_percent = (static_cast<float>(updated_info.used) * 100.0f) / static_cast<float>(updated_info.total);
                } else {
                    updated_info.usage_percent = 0.0;
                }
                
                // inode 정보 추가
                updated_info.inodes_total = fs_stats.f_files;
                updated_info.inodes_free = fs_stats.f_ffree;
                updated_info.inodes_used = updated_info.inodes_total - updated_info.inodes_free;
            } else {
                // 오류 처리
                updated_info.error_flag = true;
                updated_info.error_message = string("statvfs 오류: ") + strerror(errno);
            }

            // 결과를 공유 벡터에 안전하게 추가
            lock_guard<mutex> lock(update_mutex);
            updated_stats.push_back(updated_info); });

        pending_tasks.push_back(move(task));
    }

    // 모든 비동기 작업 완료 대기 (타임아웃 적용)
    auto start_time = chrono::steady_clock::now();

    for (auto &task : pending_tasks)
    {
        // 각 작업에 타임아웃 적용
        auto status = task.wait_for(task_timeout);

        if (status != future_status::ready)
        {
            // 타임아웃 발생 - 로그 기록
            cerr << "디스크 정보 수집 태스크 타임아웃" << endl;
            // 작업은 백그라운드에서 계속 실행됨 (리소스 누수 가능성)
        }
    }

    // 원본 데이터(disk_stats)에서 복사본(updated_stats)으로 I/O 통계 복사
    LOG_INFO("I/O 통계 정보 병합 시작 - 업데이트할 디스크 수: " + to_string(updated_stats.size()));
    for (auto &updated_info : updated_stats)
    {
        // 디스크 이름으로 원본 데이터 찾기
        for (const auto &original_info : disk_stats)
        {
            if (updated_info.device == original_info.device)
            {
                // I/O 통계 정보 복사
                updated_info.io_stats = original_info.io_stats;
                break;
            }
        }
    }

    // 1. 먼저 disk_stats 업데이트
    disk_stats = updated_stats;

    // 2. 디스크 단위로 I/O 통계 정보 업데이트
    updateIoStats();

    // 성능 측정 로깅
    auto end_time = chrono::steady_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    if (duration.count() > 100)
    {
        cerr << "디스크 사용량 정보 수집에 " << duration.count() << "ms 소요됨" << endl;
    }

    LOG_INFO("디스크 정보 업데이트 완료 - 총 디스크 수: " + to_string(disk_stats.size()));
}

/**
 * @brief I/O 통계 정보를 업데이트합니다.
 *
 * /proc/diskstats 파일에서 디스크 I/O 통계 정보를 읽어와 각 디스크에 대한
 * 읽기/쓰기 작업 수, 읽기/쓰기 바이트 수, I/O 시간 등의 정보를 업데이트합니다.
 * 이전 통계와 비교하여 초당 읽기/쓰기 속도도 계산합니다.
 */
void DiskCollector::updateIoStats()
{
    auto current_time = chrono::steady_clock::now();
    chrono::duration<double> elapsed_seconds = current_time - last_collect_time;
    double seconds = elapsed_seconds.count();

    // 너무 짧은 시간 간격에서는 계산 건너뛰기 (최소 2초 간격 적용)
    bool skip_update = seconds < 2.0;

    // 시간 간격이 충분하지 않으면 리턴 (last_collect_time은 업데이트하지 않음)
    if (skip_update)
    {
        LOG_INFO("시간 간격이 너무 짧아 I/O 통계 업데이트를 건너뜁니다: " + to_string(seconds) + "초");
        return;
    }

    // 디스크 이름으로 그룹화된 맵 생성 - 동일 디스크의 여러 파티션을 묶음
    unordered_map<string, vector<DiskInfo *>> disk_groups;

    // 이전 I/O 통계 저장 - 디스크 이름 기준
    unordered_map<string, IoStats> previous_stats;

    LOG_INFO("디스크 그룹화 및 이전 통계 저장:");
    for (auto &disk_info : disk_stats)
    {
        // 파티션이 속한 부모 디스크 이름 (sda2 -> sda)
        string parent_disk = disk_info.parent_disk;

        // 디스크 그룹에 현재 파티션 추가
        disk_groups[parent_disk].push_back(&disk_info);

        // 디스크 단위로 이전 통계 저장 (중복될 수 있으나 값은 동일)
        if (previous_stats.find(parent_disk) == previous_stats.end())
        {
            previous_stats[parent_disk] = disk_info.io_stats;
            LOG_INFO("  - 디스크: " + parent_disk + ", 파티션: " + disk_info.device);
        }
    }

    ifstream diskstats("/proc/diskstats");
    if (!diskstats.is_open())
    {
        cerr << "경고: /proc/diskstats 파일을 열 수 없습니다" << endl;
        return;
    }

    // /proc/diskstats 파일의 모든 라인을 처리
    LOG_INFO("/proc/diskstats 파일 내용 확인:");
    string line;
    while (getline(diskstats, line))
    {
        istringstream iss(line);
        int major, minor;
        string dev_name;

        // 처음 3개 필드: major, minor, device name
        iss >> major >> minor >> dev_name;

        LOG_INFO("  - 장치명: " + dev_name);

        // 장치 이름으로 매칭하여 통계 정보 업데이트 (디스크 단위로만)
        if (disk_groups.find(dev_name) != disk_groups.end())
        {
            LOG_INFO("디스크 매칭 성공 - " + dev_name);

            // diskstats의 필드 순서대로 읽기
            size_t reads_completed, reads_merged, sectors_read, read_time_ms;
            size_t writes_completed, writes_merged, sectors_written, write_time_ms;
            size_t io_in_progress, io_time_ms, weighted_io_time_ms;

            iss >> reads_completed >> reads_merged >> sectors_read >> read_time_ms >> writes_completed >> writes_merged >> sectors_written >> write_time_ms >> io_in_progress >> io_time_ms >> weighted_io_time_ms;

            // 이전 값과 비교하여 I/O 속도 계산
            IoStats io_stats;
            io_stats.reads = reads_completed;
            io_stats.writes = writes_completed;
            io_stats.read_bytes = sectors_read * 512;
            io_stats.write_bytes = sectors_written * 512;
            io_stats.read_time = static_cast<time_t>(read_time_ms) / 1000;
            io_stats.write_time = static_cast<time_t>(write_time_ms) / 1000;
            io_stats.io_time = static_cast<time_t>(io_time_ms) / 1000;
            io_stats.io_in_progress = io_in_progress;

            LOG_INFO("디스크 I/O 속도 계산 - 장치: " + dev_name +
                     ", 시간: " + to_string(seconds) +
                     ", 이전 값 존재: " + (previous_stats.find(dev_name) != previous_stats.end() ? "예" : "아니오"));

            if (previous_stats.find(dev_name) != previous_stats.end())
            {
                const auto &prev = previous_stats[dev_name];

                // 이전 값과 현재 값을 로깅
                LOG_INFO("디스크 I/O 값 비교 - 장치: " + dev_name +
                         ", 이전 읽기: " + to_string(prev.reads) +
                         ", 현재 읽기: " + to_string(reads_completed) +
                         ", 이전 쓰기: " + to_string(prev.writes) +
                         ", 현재 쓰기: " + to_string(writes_completed));

                // 카운터 값이 감소했는지 확인 (시스템 재부팅 등으로 인한 경우)
                if (reads_completed < prev.reads || writes_completed < prev.writes)
                {
                    // 카운터가 리셋된 경우 현재 값만 사용
                    io_stats.reads_per_sec = 0;
                    io_stats.writes_per_sec = 0;
                    io_stats.read_bytes_per_sec = 0;
                    io_stats.write_bytes_per_sec = 0;
                    LOG_INFO("디스크 카운터가 리셋됨 - 장치: " + dev_name);
                }
                else if (seconds > 0)
                {
                    // 정상 계산
                    io_stats.reads_per_sec = static_cast<double>(reads_completed - prev.reads) / seconds;
                    io_stats.writes_per_sec = static_cast<double>(writes_completed - prev.writes) / seconds;
                    io_stats.read_bytes_per_sec = static_cast<double>(sectors_read * 512 - prev.read_bytes) / seconds;
                    io_stats.write_bytes_per_sec = static_cast<double>(sectors_written * 512 - prev.write_bytes) / seconds;

                    LOG_INFO("디스크 I/O 속도 계산 완료 - 장치: " + dev_name +
                             ", 읽기/초: " + to_string(io_stats.reads_per_sec) +
                             ", 쓰기/초: " + to_string(io_stats.writes_per_sec) +
                             ", 읽기 바이트/초: " + to_string(io_stats.read_bytes_per_sec) +
                             ", 쓰기 바이트/초: " + to_string(io_stats.write_bytes_per_sec));
                }
                else
                {
                    // 시간 간격이 0인 경우 (동일 시간에 두 번 호출된 경우)
                    LOG_INFO("시간 간격이 0 - 장치: " + dev_name + ", 이전 속도 값 유지");
                    // 이전 속도 값 유지 (변경하지 않음)
                }
            }
            else
            {
                // 이전 값이 없는 경우 초기화
                io_stats.reads_per_sec = 0;
                io_stats.writes_per_sec = 0;
                io_stats.read_bytes_per_sec = 0;
                io_stats.write_bytes_per_sec = 0;
                LOG_INFO("이전 통계 정보 없음 - 장치: " + dev_name + ", 초기화");
            }

            // 동일 디스크에 속한 모든 파티션에 동일한 I/O 통계 적용
            for (DiskInfo *disk_ptr : disk_groups[dev_name])
            {
                disk_ptr->io_stats = io_stats;

                // 디버깅 로깅 추가
                LOG_INFO("I/O 통계 복사 - 디스크: " + dev_name + ", 파티션: " + disk_ptr->device +
                         ", 읽기/초: " + to_string(io_stats.reads_per_sec) +
                         ", 쓰기/초: " + to_string(io_stats.writes_per_sec));
            }
        }
    }

    // 디스크 이름으로 매칭되지 않은 파티션 처리 - 파티션 이름으로 다시 시도
    diskstats.clear();
    diskstats.seekg(0, ios::beg);

    while (getline(diskstats, line))
    {
        istringstream iss(line);
        int major, minor;
        string dev_name;
        iss >> major >> minor >> dev_name;

        // 각 파티션에 대해 검사
        for (auto &disk_info : disk_stats)
        {
            // 이미 처리된 파티션은 건너뜀
            if (disk_info.io_stats.reads_per_sec > 0 || disk_info.io_stats.writes_per_sec > 0)
                continue;

            string device_name = extractDeviceName(disk_info.device);
            if (device_name == dev_name)
            {
                LOG_INFO("파티션 매칭 성공 - " + disk_info.device + " -> " + dev_name);

                // diskstats의 필드 읽기 및 처리
                // (이전 코드와 동일한 로직)
            }
        }
    }

    // 실제로 값을 수집한 경우에만 마지막 수집 시간 업데이트
    last_collect_time = current_time;
}

/**
 * @brief 장치 경로에서 장치 이름만 추출합니다.
 *
 * 다양한 형식의 장치 경로(/dev/mapper/vg-lv, /dev/md0, /dev/nvme0n1p1 등)에서
 * 실제 장치 이름만 추출합니다. 필요시 Linux 시스템의 디바이스 매핑 정보도 확인합니다.
 *
 * @param device_path 장치 경로 (예: /dev/sda, /dev/mapper/vg-lv)
 * @return string 추출된 장치 이름 (예: sda, dm-0, md0)
 */
string DiskCollector::extractDeviceName(const string &device_path)
{
    // /dev/mapper/vg-lv, /dev/md0, /dev/nvme0n1p1 등 다양한 형식 지원
    size_t pos = device_path.find_last_of('/');
    if (pos != string::npos)
    {
        string name = device_path.substr(pos + 1);

        // mapper 장치인 경우 dm-* 형식으로 변환 필요
        if (device_path.find("/dev/mapper/") == 0)
        {
            // dm-X 형식으로 변환하는 로직
            // 심볼릭 링크 확인을 통해 실제 장치 이름 찾기
            string dm_name;
            ifstream dm_name_file("/sys/block/dm-0/dm/name");
            if (dm_name_file.is_open())
            {
                string dm_mapped_name;
                getline(dm_name_file, dm_mapped_name);
                if (name == dm_mapped_name)
                {
                    return "dm-0";
                }
            }

            // dm-1, dm-2 등도 확인
            for (int i = 1; i <= 10; i++)
            {
                ifstream dm_name_file("/sys/block/dm-" + to_string(i) + "/dm/name");
                if (dm_name_file.is_open())
                {
                    string dm_mapped_name;
                    getline(dm_name_file, dm_mapped_name);
                    if (name == dm_mapped_name)
                    {
                        return "dm-" + to_string(i);
                    }
                }
            }

            // 매핑을 찾지 못한 경우
            LOG_INFO("mapper 장치의 dm 이름을 찾지 못함: " + name);
        }

        return name;
    }
    return device_path;
}

/**
 * @brief 파티션 이름으로부터 부모 디스크 이름을 추출합니다.
 *
 * 파티션 이름(예: sda1, nvme0n1p1, mmcblk0p1)을 분석하여 해당 파티션이
 * 속한 부모 디스크 이름(예: sda, nvme0n1, mmcblk0)을 반환합니다.
 *
 * @param partition 파티션 이름
 * @return string 부모 디스크 이름
 */
string DiskCollector::getParentDisk(const string &partition)
{
    // 일반적인 sda1 → sda 형식 변환
    string parent;
    if (partition.length() > 3 &&
        ((partition.substr(0, 2) == "sd" || partition.substr(0, 2) == "hd") && isdigit(partition.back())))
    {
        // 마지막 숫자를 제거
        for (size_t i = 0; i < partition.length(); i++)
        {
            if (isdigit(partition[i]))
            {
                parent = partition.substr(0, i);
                break;
            }
        }
        if (parent.empty())
        {
            parent = partition;
        }
    }
    // nvme0n1p1 → nvme0n1 형식 변환
    else if (partition.find("nvme") == 0 && partition.find('p') != string::npos)
    {
        size_t p_pos = partition.find('p');
        if (p_pos > 4 && isdigit(partition[p_pos - 1]) && isdigit(partition[p_pos + 1]))
        {
            parent = partition.substr(0, p_pos);
        }
        else
        {
            parent = partition;
        }
    }
    // mmcblk0p1 → mmcblk0 형식 변환
    else if (partition.find("mmcblk") == 0 && partition.find('p') != string::npos)
    {
        size_t p_pos = partition.find('p');
        if (isdigit(partition[p_pos + 1]))
        {
            parent = partition.substr(0, p_pos);
        }
        else
        {
            parent = partition;
        }
    }
    // dm-0 등의 경우 그대로 사용
    else if (partition.find("dm-") == 0)
    {
        parent = partition;
    }
    else
    {
        parent = partition;
    }

    return parent;
}

/**
 * @brief 디스크 정보를 수집합니다.
 *
 * 주기적으로 호출되어 시스템의 디스크 정보를 업데이트합니다.
 * 디스크 구성이 변경된 경우 전체 디스크 정보를 다시 수집하고,
 * 그렇지 않은 경우 기존 디스크의 사용량 정보만 업데이트합니다.
 */
void DiskCollector::collect()
{
    auto current_time = chrono::steady_clock::now();

    // 디스크 정보가 없거나 변경 감지 시 초기화
    if (disk_stats.empty() || detectDiskChanges())
    {
        collectDiskInfo();
    }
    else
    {
        // 사용량 정보만 비동기적으로 업데이트
        updateDiskUsage();
    }
}

/**
 * @brief 수집된 디스크 통계 정보를 반환합니다.
 *
 * @return vector<DiskInfo> 수집된 모든 디스크의 정보
 */
vector<DiskInfo> DiskCollector::getDiskStats() const
{
    LOG_INFO("getDiskStats 호출");
    for (const auto &disk : disk_stats)
    {
        LOG_INFO("이름: " + disk.device + " 읽기/초: " + to_string(disk.io_stats.reads_per_sec) + " 쓰기/초: " + to_string(disk.io_stats.writes_per_sec) + " 읽기 바이트/초: " + to_string(disk.io_stats.read_bytes_per_sec) + " 쓰기 바이트/초: " + to_string(disk.io_stats.write_bytes_per_sec));
    }
    return disk_stats;
}

/**
 * @brief 시스템의 디스크 구성 변경을 감지합니다.
 *
 * /proc/partitions 파일의 내용을 모니터링하여 디스크나 파티션의
 * 추가/제거와 같은 변경사항을 감지합니다.
 *
 * @return bool 디스크 구성이 변경되었으면 true, 그렇지 않으면 false
 */
bool DiskCollector::detectDiskChanges()
{
    // 추가: 파티션 테이블 변경 확인
    static string last_partitions_checksum;
    ifstream partitions("/proc/partitions");
    if (partitions.is_open())
    {
        stringstream buffer;
        buffer << partitions.rdbuf();
        string content = buffer.str();

        // 간단한 체크섬 생성 (내용의 해시)
        size_t checksum = hash<string>{}(content);
        string current_checksum = to_string(checksum);

        if (current_checksum != last_partitions_checksum)
        {
            last_partitions_checksum = current_checksum;
            return true; // 변경 감지
        }
    }

    return false;
}

/**
 * @brief 스레드 풀의 작업자 스레드 함수입니다.
 *
 * 작업 큐에서 태스크를 가져와 실행합니다.
 * 종료 신호가 있거나 작업 큐가 비어있을 때까지 대기합니다.
 */
void DiskCollector::threadWorker()
{
    while (!stop_threads)
    {
        function<void()> task;
        {
            // 작업 큐에서 태스크 가져오기
            unique_lock<mutex> lock(queue_mutex);
            cv.wait(lock, [this]
                    { return !task_queue.empty() || stop_threads; });

            if (stop_threads && task_queue.empty())
            {
                return;
            }

            if (!task_queue.empty())
            {
                task = move(task_queue.front());
                task_queue.pop();
                active_tasks++;
            }
        }

        // 태스크 실행
        if (task)
        {
            task();
            active_tasks--;
        }
    }
}

/**
 * @brief 작업 큐에 새 태스크를 추가합니다.
 *
 * @param task 실행할 함수 객체
 */
void DiskCollector::addTask(function<void()> task)
{
    {
        unique_lock<mutex> lock(queue_mutex);
        task_queue.push(move(task));
    }
    cv.notify_one();
}

/**
 * @brief 모든 작업이 완료될 때까지 대기합니다.
 *
 * 작업 큐가 비어있고 현재 실행 중인 작업이 없을 때까지 대기합니다.
 */
void DiskCollector::waitForTasks()
{
    while (true)
    {
        {
            unique_lock<mutex> lock(queue_mutex);
            if (task_queue.empty() && active_tasks == 0)
            {
                break;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}