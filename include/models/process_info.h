#pragma once
#include <string>

using namespace std;

/**
 * @brief 시스템에서 실행 중인 프로세스의 상세 정보를 저장하는 구조체
 *
 * 이 구조체는 프로세스 모니터링에 필요한 모든 핵심 정보를 포함합니다.
 * CPU 사용량, 메모리 사용량, I/O 활동 및 기타 프로세스 관련 정보를 추적합니다.
 */
struct ProcessInfo
{
    /** @brief 프로세스 식별자 (Process ID) */
    pid_t pid;

    /** @brief 부모 프로세스 식별자 (Parent Process ID) */
    pid_t ppid;

    /** @brief 프로세스 이름 */
    string name;

    /** @brief 프로세스 상태 (실행 중, 대기, 중지 등) */
    string status;

    /** @brief CPU 사용률 (%) */
    float cpu_usage;

    /** @brief 실제 물리 메모리 사용량 (Resident Set Size, KB 단위) */
    uint64_t memory_rss;

    /** @brief 가상 메모리 사용량 (Virtual Memory Size, KB 단위) */
    uint64_t memory_vsz;

    /** @brief 프로세스의 스레드 개수 */
    int threads;

    /** @brief 프로세스를 실행한 사용자 이름 */
    string user;

    /** @brief 프로세스 실행 명령어 및 매개변수 */
    string command;

    /** @brief 프로세스 시작 시간 (타임스탬프) */
    time_t start_time;

    /** @brief 누적 CPU 사용 시간 (초 단위) */
    float cpu_time;

    /** @brief 읽은 I/O 바이트 수 */
    uint64_t io_read_bytes;

    /** @brief 쓴 I/O 바이트 수 */
    uint64_t io_write_bytes;

    /** @brief 프로세스가 열어놓은 파일 개수 */
    int open_files;

    /** @brief 프로세스 우선순위 값 (-20에서 19 사이, 낮을수록 우선순위 높음) */
    int nice;
};