/**
 * @file docker_container_info.h
 * @brief Docker 컨테이너 정보를 표현하는 구조체들을 정의합니다.
 */

#pragma once

#include <string>
#include <map>
#include <vector>

using namespace std;

/**
 * @struct DockerContainerHealth
 * @brief Docker 컨테이너의 상태 정보를 저장하는 구조체
 */
struct DockerContainerHealth
{
    /// @brief 컨테이너 상태 (예: healthy, unhealthy)
    string status;
    /// @brief 상태 점검 실패 횟수
    int failing_streak;
    /// @brief 마지막 상태 점검 출력 결과
    string last_check_output;
};

/**
 * @struct DockerContainerPort
 * @brief Docker 컨테이너의 포트 매핑 정보를 저장하는 구조체
 */
struct DockerContainerPort
{
    /// @brief 컨테이너 내부 포트
    string container_port;
    /// @brief 호스트 포트
    string host_port;
    /// @brief 프로토콜 (예: tcp, udp)
    string protocol;
};

/**
 * @struct DockerContainerNetwork
 * @brief Docker 컨테이너의 네트워크 설정 정보를 저장하는 구조체
 */
struct DockerContainerNetwork
{
    /// @brief 네트워크 이름
    string network_name;
    /// @brief 네트워크 IP 주소
    string network_ip;
    /// @brief 네트워크 MAC 주소
    string network_mac;
    /// @brief 수신된 데이터 크기
    string network_rx_bytes;
    /// @brief 전송된 데이터 크기
    string network_tx_bytes;
};

/**
 * @struct DockerContainerVolume
 * @brief Docker 컨테이너의 볼륨 마운트 정보를 저장하는 구조체
 */
struct DockerContainerVolume
{
    /// @brief 호스트 볼륨 경로
    string source;
    /// @brief 컨테이너 내부 경로
    string destination;
    /// @brief 마운트 모드 (예: ro, rw)
    string mode;
    /// @brief 볼륨 타입
    string type;
};

/**
 * @struct DockerContainerEnv
 * @brief Docker 컨테이너의 환경 변수 정보를 저장하는 구조체
 */
struct DockerContainerEnv
{
    /// @brief 환경 변수 키
    string key;
    /// @brief 환경 변수 값
    string value;
};

/**
 * @struct DockerContainerLabel
 * @brief Docker 컨테이너의 라벨 정보를 저장하는 구조체
 */
struct DockerContainerLabel
{
    /// @brief 라벨 키
    string label_key;
    /// @brief 라벨 값
    string label_value;
};

/**
 * @struct DockerContainerInfo
 * @brief Docker 컨테이너의 종합적인 정보를 저장하는 구조체
 */
struct DockerContainerInfo
{
    /// @brief 컨테이너 ID
    string container_id;
    /// @brief 컨테이너 이름
    string container_name;
    /// @brief 컨테이너 이미지
    string container_image;
    /// @brief 컨테이너 상태 (예: running, exited)
    string container_status;
    /// @brief 컨테이너 생성 시간
    string container_created;
    /// @brief 컨테이너 상태 정보
    DockerContainerHealth container_health;
    /// @brief 컨테이너 포트 매핑 정보 목록
    vector<DockerContainerPort> container_ports;
    /// @brief 컨테이너 네트워크 정보 목록
    vector<DockerContainerNetwork> container_network;
    /// @brief 컨테이너 볼륨 마운트 정보 목록
    vector<DockerContainerVolume> container_volumes;
    /// @brief 컨테이너 환경 변수 목록
    vector<DockerContainerEnv> container_env;
    /// @brief CPU 사용률 (%)
    double cpu_usage;
    /// @brief 메모리 사용량 (바이트)
    uint64_t memory_usage;
    /// @brief 메모리 제한 (바이트)
    uint64_t memory_limit;
    /// @brief 메모리 사용률 (%)
    double memory_percent;
    /// @brief 실행 명령어
    string command;
    /// @brief 네트워크 수신 데이터 (바이트)
    uint64_t network_rx_bytes;
    /// @brief 네트워크 전송 데이터 (바이트)
    uint64_t network_tx_bytes;
    /// @brief 블록 디바이스 읽기 (바이트)
    uint64_t block_read;
    /// @brief 블록 디바이스 쓰기 (바이트)
    uint64_t block_write;
    /// @brief 프로세스 ID 수
    int pids;
    /// @brief 재시작 횟수
    int restarts;
    /// @brief 컨테이너 라벨 목록
    vector<DockerContainerLabel> labels;

    /**
     * @brief 기본 생성자
     * @details 모든 숫자 필드를 0으로 초기화하고 컨테이너 상태 정보의 실패 횟수를 0으로 설정합니다.
     */
    DockerContainerInfo()
        : cpu_usage(0.0f), memory_usage(0), memory_limit(0), memory_percent(0.0f),
          network_rx_bytes(0), network_tx_bytes(0), block_read(0), block_write(0),
          pids(0), restarts(0)
    {
        container_health.failing_streak = 0;
    }
};