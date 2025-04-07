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

    // 각 디스크에 대해 비동기 작업 생성
    for (auto &disk_info : disk_stats)
    {
        // 비동기 태스크 실행
        std::future<void> task = std::async(std::launch::async, [&disk_info]()
                                            {
            struct statvfs fs_stats;
            if (statvfs(disk_info.mount_point.c_str(), &fs_stats) == 0) {
                // 전체 용량 (바이트 단위)
                disk_info.total = fs_stats.f_blocks * fs_stats.f_frsize;
                // 사용 가능한 용량
                disk_info.free = fs_stats.f_bfree * fs_stats.f_frsize;
                // 사용중인 용량
                disk_info.used = disk_info.total - disk_info.free;
                
                // 사용률 계산 (백분율)
                if (disk_info.total > 0) {
                    disk_info.usage_percent = (static_cast<float>(disk_info.used) * 100.0f) / static_cast<float>(disk_info.total);
                } else {
                    disk_info.usage_percent = 0.0;
                }
                
                // inode 정보 추가
                disk_info.inodes_total = fs_stats.f_files;
                disk_info.inodes_free = fs_stats.f_ffree;
                disk_info.inodes_used = disk_info.inodes_total - disk_info.inodes_free;
            } else {
                // 오류 처리
                disk_info.error_flag = true;
                disk_info.error_message = string("statvfs 오류: ") + strerror(errno);
            } });

        pending_tasks.push_back(std::move(task));
    }

    // 모든 비동기 작업 완료 대기 (타임아웃 적용)
    auto start_time = std::chrono::steady_clock::now();

    for (auto &task : pending_tasks)
    {
        // 각 작업에 타임아웃 적용
        auto status = task.wait_for(task_timeout);

        if (status != std::future_status::ready)
        {
            // 타임아웃 발생 - 로그 기록
            std::cerr << "디스크 정보 수집 태스크 타임아웃" << std::endl;
            // 작업은 백그라운드에서 계속 실행됨 (리소스 누수 가능성)
        }
    }

    // I/O 통계 정보 업데이트
    updateIoStats();

    // 성능 측정 로깅
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (duration.count() > 100)
    {
        std::cerr << "디스크 사용량 정보 수집에 " << duration.count() << "ms 소요됨" << std::endl;
    }
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

    // 이전 I/O 통계 저장
    unordered_map<string, IoStats> previous_stats;
    for (const auto &disk : disk_stats)
    {
        size_t pos = disk.device.find_last_of('/');
        string device_name = (pos != string::npos) ? disk.device.substr(pos + 1) : disk.device;
        previous_stats[device_name] = disk.io_stats;
    }

    ifstream diskstats("/proc/diskstats");
    if (!diskstats.is_open())
    {
        cerr << "경고: /proc/diskstats 파일을 열 수 없습니다" << endl;
        for (auto &disk_info : disk_stats)
        {
            disk_info.io_stats.error_flag = true; // 오류 상태 표시
        }
        return;
    }

    string line;
    while (getline(diskstats, line))
    {
        istringstream iss(line);
        int major, minor;
        string dev_name;

        // 처음 3개 필드: major, minor, device name
        iss >> major >> minor >> dev_name;

        // 장치 이름으로 매칭하여 통계 정보 업데이트
        for (auto &disk_info : disk_stats)
        {
            // 장치 경로에서 장치 이름만 추출 (예: /dev/sda -> sda)
            string device_name = extractDeviceName(disk_info.device);

            if (device_name == dev_name)
            {
                // diskstats의 필드 순서대로 읽기 (Linux 커널 문서 참조)
                size_t reads_completed, reads_merged, sectors_read, read_time_ms;
                size_t writes_completed, writes_merged, sectors_written, write_time_ms;
                size_t io_in_progress, io_time_ms, weighted_io_time_ms;

                iss >> reads_completed >> reads_merged >> sectors_read >> read_time_ms >> writes_completed >> writes_merged >> sectors_written >> write_time_ms >> io_in_progress >> io_time_ms >> weighted_io_time_ms;

                // IoStats 구조체에 데이터 저장
                disk_info.io_stats.reads = reads_completed;
                disk_info.io_stats.writes = writes_completed;
                disk_info.io_stats.read_bytes = sectors_read * 512; // 섹터 크기는 일반적으로 512 바이트
                disk_info.io_stats.write_bytes = sectors_written * 512;
                disk_info.io_stats.read_time = static_cast<time_t>(read_time_ms) / 1000;
                disk_info.io_stats.write_time = static_cast<time_t>(write_time_ms) / 1000;
                disk_info.io_stats.io_time = static_cast<time_t>(io_time_ms) / 1000;
                disk_info.io_stats.io_in_progress = io_in_progress;

                // 이전 값과 비교하여 I/O 속도 계산
                if (previous_stats.find(device_name) != previous_stats.end() && seconds > 0)
                {
                    const auto &prev = previous_stats[device_name];
                    disk_info.io_stats.reads_per_sec = static_cast<double>(reads_completed - prev.reads) / seconds;
                    disk_info.io_stats.writes_per_sec = static_cast<double>(writes_completed - prev.writes) / seconds;
                    disk_info.io_stats.read_bytes_per_sec = static_cast<double>(sectors_read * 512 - prev.read_bytes) / seconds;
                    disk_info.io_stats.write_bytes_per_sec = static_cast<double>(sectors_written * 512 - prev.write_bytes) / seconds;
                }

                break; // 일치하는 장치를 찾았으므로 루프 종료
            }
        }
    }

    // 파티션과 디스크 매핑 생성
    unordered_map<string, vector<string>> disk_partitions;
    for (const auto &disk : disk_stats)
    {
        string device_name = extractDeviceName(disk.device);
        string parent_disk = getParentDisk(device_name);

        if (parent_disk != device_name)
        {
            disk_partitions[parent_disk].push_back(device_name);
        }
    }

    // 필요시 부모 디스크 정보로 파티션 정보 보완
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
            // 실제 dm 장치 이름 찾기 로직 필요
            // (간단한 구현은 /sys/block/ 디렉토리 검사 필요)
        }

        // RAID/MD 장치 처리 추가
        if (device_path.find("/dev/md") == 0)
        {
            // 기본적으로 md 장치는 그대로 사용하되 번호만 추출
            string md_name = device_path.substr(device_path.find_last_of('/') + 1);

            // md 장치의 실제 구성요소 확인 (선택적)
            ifstream md_stat("/proc/mdstat");
            if (md_stat.is_open())
            {
                string line;
                bool found_md = false;
                while (getline(md_stat, line))
                {
                    if (line.find(md_name + " :") == 0)
                    {
                        found_md = true;
                        // 여기서 RAID 정보를 추출할 수 있음
                        // 실제 구현에서는 RAID 레벨, 상태 등을 저장할 수 있음
                    }
                }
                if (found_md)
                {
                    return md_name;
                }
            }
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
    last_collect_time = current_time;

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
        std::function<void()> task;
        {
            // 작업 큐에서 태스크 가져오기
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [this]
                    { return !task_queue.empty() || stop_threads; });

            if (stop_threads && task_queue.empty())
            {
                return;
            }

            if (!task_queue.empty())
            {
                task = std::move(task_queue.front());
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
void DiskCollector::addTask(std::function<void()> task)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        task_queue.push(std::move(task));
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
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (task_queue.empty() && active_tasks == 0)
            {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}