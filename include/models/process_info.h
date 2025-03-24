#pragma once
#include <string>

using namespace std;

struct ProcessInfo
{
    pid_t pid;               // 프로세스 식별자 (Process ID)
    pid_t ppid;              // 부모 프로세스 식별자 (Parent Process ID)
    string name;             // 프로세스 이름
    string status;           // 프로세스 상태 (실행 중, 대기, 중지 등)
    float cpu_usage;         // CPU 사용률 (%)
    uint64_t memory_rss;     // 실제 물리 메모리 사용량 (Resident Set Size, KB 단위)
    uint64_t memory_vsz;     // 가상 메모리 사용량 (Virtual Memory Size, KB 단위)
    int threads;             // 프로세스의 스레드 개수
    string user;             // 프로세스를 실행한 사용자 이름
    string command;          // 프로세스 실행 명령어 및 매개변수
    time_t start_time;       // 프로세스 시작 시간 (타임스탬프)
    float cpu_time;          // 누적 CPU 사용 시간 (초 단위)
    uint64_t io_read_bytes;  // 읽은 I/O 바이트 수
    uint64_t io_write_bytes; // 쓴 I/O 바이트 수
    int open_files;          // 프로세스가 열어놓은 파일 개수
    int nice;                // 프로세스 우선순위 값 (-20에서 19 사이, 낮을수록 우선순위 높음)
};