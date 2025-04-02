/**
 * @file dashboard_ncurses.h
 * @brief Ncurses 기반 시스템 모니터링 대시보드 클래스 헤더
 * @author 시스템 모니터 개발팀
 */
#pragma once
#include "collectors/cpu_collector.h"
#include "collectors/memory_collector.h"
#include "collectors/disk_collector.h"
#include "collectors/network_collector.h"
#include "collectors/systeminfo_collector.h"
#include "collectors/process_collector.h"
#include "collectors/docker_collector.h"
#include "i_dashboard.h"
#include "models/system_metrics.h"
#include <memory>
#include <atomic>

using namespace std;

/**
 * @brief 대시보드 실행 상태를 제어하는 전역 변수
 * @details 모든 스레드에서 안전하게 접근할 수 있는 원자적 변수
 */
extern atomic<bool> running; // extern으로 선언 (참조 기호 & 제거)

/**
 * @class DashboardNcurses
 * @brief Ncurses 라이브러리를 사용한 시스템 모니터링 대시보드 구현
 * @details IDashboard 인터페이스를 구현하여 터미널 환경에서 시스템 상태를 시각적으로 표시
 */
class DashboardNcurses : public IDashboard
{
public:
    /**
     * @brief 기본 생성자
     * @details DashboardNcurses 객체를 초기화하고 필요한 리소스 할당
     */
    DashboardNcurses();

    /**
     * @brief 소멸자
     * @details 할당된 리소스 해제 및 ncurses 환경 정리
     */
    ~DashboardNcurses() override;

    /**
     * @brief 대시보드 초기화 함수
     * @details ncurses 환경 설정 및 화면 초기화 수행
     */
    void init() override;

    /**
     * @brief 대시보드 업데이트 함수
     * @details 시스템 정보 수집 및 화면 갱신 수행
     */
    void update() override;

    /**
     * @brief 사용자 입력 처리 함수
     * @details 키보드 입력을 감지하고 적절한 동작 수행
     */
    void handleInput() override;

    /**
     * @brief 대시보드 종료 정리 함수
     * @details ncurses 환경 종료 및 리소스 정리
     */
    void cleanup() override;

private:
    /**
     * @brief 현재 커서 위치 행 번호
     */
    int row = 0;

    /**
     * @brief 표시할 정보 타입 식별자
     */
    int view_info = 0;

    /**
     * @brief 현재 페이지 번호
     */
    size_t current_page = 0;

    /**
     * @brief 페이지당 표시할 프로세스 수
     */
    size_t processes_per_page = 0;

    /**
     * @brief 정렬 기준 식별자
     */
    int sort_by = 0;

    /**
     * @brief 화면 내용 지우기 함수
     * @details ncurses 화면을 초기화
     */
    void clearScreen();

    /**
     * @brief 시스템 지표 데이터 모델
     */
    SystemMetrics system_metrics;

    /**
     * @brief CPU 정보 수집기
     */
    CPUCollector cpu_collector;

    /**
     * @brief 메모리 정보 수집기
     */
    MemoryCollector memory_collector;

    /**
     * @brief 디스크 정보 수집기
     */
    DiskCollector disk_collector;

    /**
     * @brief 네트워크 정보 수집기
     */
    NetworkCollector network_collector;

    /**
     * @brief 시스템 기본 정보 수집기
     */
    SystemInfoCollector systeminfo_collector;

    /**
     * @brief 프로세스 정보 수집기
     */
    ProcessCollector process_collector;

    /**
     * @brief 도커 컨테이너 정보 수집기
     */
    DockerCollector docker_collector;

    /**
     * @brief 시스템 자원 모니터링 화면 업데이트 함수
     * @details CPU, 메모리, 디스크, 네트워크 상태 화면 갱신
     */
    void updateMonitor();

    /**
     * @brief 시스템 정보 화면 업데이트 함수
     * @details 운영체제, 하드웨어 정보 등 기본 시스템 정보 화면 갱신
     */
    void updateSystemInfo();

    /**
     * @brief 프로세스 목록 화면 업데이트 함수
     * @details 실행 중인 프로세스 목록과 자원 사용량 화면 갱신
     */
    void updateProcesses();

    /**
     * @brief 도커 컨테이너 화면 업데이트 함수
     * @details 실행 중인 도커 컨테이너 정보 화면 갱신
     */
    void updateDocker();

    /**
     * @brief 구분선 생성 함수
     * @param title 구분선 중앙에 표시할 제목 (기본값: 빈 문자열)
     * @return 생성된 구분선 문자열
     */
    string getDivider(const string &title = "");
};