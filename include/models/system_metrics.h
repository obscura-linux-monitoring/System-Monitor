/**
 * @file system_metrics.h
 * @brief 시스템 메트릭 정보를 담는 구조체 정의
 * @author 시스템 모니터링 팀
 */

#pragma once

#include <string>
#include <vector>

#include "models/cpu_info.h"
#include "models/memory_info.h"
#include "models/disk_info.h"
#include "models/network_interface.h"
#include "models/process_info.h"
#include "models/docker_container_info.h"
#include "models/system_info.h"
#include "models/service_info.h"

using namespace std;

/**
 * @struct SystemMetrics
 * @brief 시스템의 전체 메트릭 데이터를 포함하는 구조체
 *
 * 이 구조체는 시스템 리소스 사용량, 프로세스 정보, 도커 컨테이너 정보 등
 * 시스템 모니터링에 필요한 모든 데이터를 포함합니다.
 */
struct SystemMetrics
{
    /**
     * @brief 사용자 식별자
     */
    string user_id;

    /**
     * @brief 인증 키
     */
    string key;

    /**
     * @brief 데이터 수집 시간 타임스탬프
     */
    string timestamp;

    /**
     * @brief 시스템 기본 정보
     */
    SystemInfo system;

    /**
     * @brief CPU 사용량 정보
     */
    CpuInfo cpu;

    /**
     * @brief 메모리 사용량 정보
     */
    MemoryInfo memory;

    /**
     * @brief 디스크 정보 목록
     */
    vector<DiskInfo> disk;

    /**
     * @brief 네트워크 인터페이스 정보 목록
     */
    vector<NetworkInterface> network;

    /**
     * @brief 실행 중인 프로세스 정보 목록
     */
    vector<ProcessInfo> process;

    /**
     * @brief 도커 컨테이너 정보 목록
     */
    vector<DockerContainerInfo> docker;

    /**
     * @brief 시스템 서비스 정보 목록
     */
    vector<ServiceInfo> services;
};