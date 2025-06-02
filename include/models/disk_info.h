/**
 * @file disk_info.h
 * @brief 디스크 및 입출력 정보를 포함하는 구조체 정의
 * @author System Monitor Team
 */

#pragma once

#include <string>

using namespace std;

/**
 * @brief 디스크 입출력(I/O) 통계 정보를 저장하는 구조체
 *
 * 디스크 읽기/쓰기 작업, 처리된 바이트 수, 소요 시간 등의
 * 입출력 관련 통계 데이터를 포함합니다.
 */
struct IoStats
{
    size_t reads;               ///< 총 읽기 작업 횟수
    size_t writes;              ///< 총 쓰기 작업 횟수
    size_t read_bytes;          ///< 총 읽은 바이트 수
    size_t write_bytes;         ///< 총 쓴 바이트 수
    time_t read_time;           ///< 읽기 작업에 소요된 시간(ms)
    time_t write_time;          ///< 쓰기 작업에 소요된 시간(ms)
    time_t io_time;             ///< I/O 작업에 소요된 총 시간(ms)
    size_t io_in_progress;      ///< 현재 진행 중인 I/O 작업 수
    double reads_per_sec;       ///< 초당 읽기 작업 수
    double writes_per_sec;      ///< 초당 쓰기 작업 수
    double read_bytes_per_sec;  ///< 초당 읽은 바이트 수
    double write_bytes_per_sec; ///< 초당 쓴 바이트 수
    bool error_flag;            ///< 오류 발생 여부 플래그
    /**
     * @brief IoStats 구조체의 기본 생성자
     *
     * 모든 수치 값을 0으로, error_flag를 false로 초기화합니다.
     */
    IoStats() : reads(0), writes(0), read_bytes(0), write_bytes(0),
                read_time(0), write_time(0), io_time(0), io_in_progress(0),
                reads_per_sec(0), writes_per_sec(0),
                read_bytes_per_sec(0), write_bytes_per_sec(0),
                error_flag(false) {}
};

/**
 * @brief 디스크 장치 정보를 저장하는 구조체
 *
 * 디스크 장치, 마운트 지점, 파일 시스템 정보, 용량 통계,
 * I/O 통계 및 오류 정보를 포함합니다.
 */
struct DiskInfo
{
    string device;          ///< 디스크 장치 경로 (예: /dev/sda1)
    string mount_point;     ///< 디스크 마운트 지점 (예: /home)
    string filesystem_type; ///< 파일 시스템 유형 (예: ext4, ntfs)
    size_t total;           ///< 디스크 총 용량 (바이트)
    size_t used;            ///< 사용 중인 디스크 공간 (바이트)
    size_t free;            ///< 사용 가능한 디스크 공간 (바이트)
    float usage_percent;    ///< 디스크 사용률 (백분율)
    size_t inodes_total;    ///< 총 inode 수
    size_t inodes_used;     ///< 사용 중인 inode 수
    size_t inodes_free;     ///< 사용 가능한 inode 수
    IoStats io_stats;       ///< 디스크 I/O 통계 정보
    bool error_flag;        ///< 오류 발생 여부 플래그
    string error_message;   ///< 오류 발생 시 오류 메시지
    string model_name;      ///< 모델 이름
    string type;            ///< 디스크 유형
    bool is_system_disk;    ///< 시스템 디스크 여부
    bool is_page_file_disk; ///< 페이지 파일 디스크 여부
    string parent_disk;     ///< 부모 디스크 이름
};