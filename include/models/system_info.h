/**
 * @file system_info.h
 * @brief 시스템 정보를 저장하기 위한 구조체 정의
 */
#pragma once

#include <string>
#include <ctime>

using namespace std;

/**
 * @brief 시스템 정보를 저장하는 구조체
 *
 * 호스트명, 운영체제 정보, 가동 시간 등 시스템 관련 정보를 포함합니다.
 */
struct SystemInfo
{
    /** @brief 시스템의 호스트명 */
    string hostname;
    /** @brief 운영체제 이름 */
    string os_name;
    /** @brief 운영체제 버전 */
    string os_version;
    /** @brief 운영체제 커널 버전 */
    string os_kernel_version;
    /** @brief 운영체제 아키텍처 (예: x86_64, arm64) */
    string os_architecture;
    /** @brief 시스템 가동 시간 (초 단위) */
    time_t uptime;
    /** @brief 시스템 부팅 시간 (Unix 타임스탬프) */
    time_t boot_time;
    /** @brief 시스템 전체 프로세스 갯수 */
    unsigned int total_processes;
    /** @brief 시스템 전체 스레드 갯수 */
    unsigned int total_threads;
    /** @brief 시스템 전체 열린 파일 디스크립터 갯수 */
    unsigned int total_file_descriptors;
};