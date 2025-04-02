#pragma once
/**
 * @file process_collector.h
 * @brief 시스템 프로세스 정보 수집 및 관리를 위한 클래스 정의
 */
#include "collector.h"
#include "../models/process_info.h"
#include <vector>
#include <map>
#include <string>
#include <string_view>
#include <functional>

using namespace std;

/**
 * @class ProcessCollector
 * @brief 시스템 프로세스 정보를 수집하고 관리하는 클래스
 *
 * 이 클래스는 시스템의 모든 프로세스 정보를 수집하고 다양한 기준으로
 * 정렬하여 제공합니다. CPU 사용량, 메모리 사용량, PID, 이름 등으로
 * 프로세스를 정렬할 수 있습니다.
 */
class ProcessCollector : public Collector
{
private:
    /**
     * @brief 수집된 프로세스 정보 목록
     * @note 캐시 라인 정렬(64바이트)을 통해 성능 최적화
     */
    alignas(64) vector<ProcessInfo> processes;

    /**
     * @brief 이전 CPU 시간 측정값을 저장하는 맵 (PID별)
     * @note 캐시 라인 정렬(64바이트)을 통해 성능 최적화
     */
    alignas(64) map<pid_t, pair<unsigned long, unsigned long>> prev_cpu_times;

    /**
     * @brief 이전 총 CPU 시간
     * @note 캐시 라인 정렬(64바이트)을 통해 성능 최적화
     */
    alignas(64) unsigned long prev_total_time = 0;

    /**
     * @brief CPU 사용량 기준으로 정렬된 프로세스 목록 반환
     * @return CPU 사용량 기준으로 정렬된 프로세스 목록
     */
    vector<ProcessInfo> getProcessesByCpuUsage() const;

    /**
     * @brief 메모리 사용량 기준으로 정렬된 프로세스 목록 반환
     * @return 메모리 사용량 기준으로 정렬된 프로세스 목록
     */
    vector<ProcessInfo> getProcessesByMemoryUsage() const;

    /**
     * @brief PID 기준으로 정렬된 프로세스 목록 반환
     * @return PID 기준으로 정렬된 프로세스 목록
     */
    vector<ProcessInfo> getProcessesByPid() const;

    /**
     * @brief 이름 기준으로 정렬된 프로세스 목록 반환
     * @return 이름 기준으로 정렬된 프로세스 목록
     */
    vector<ProcessInfo> getProcessesByName() const;

    /**
     * @brief 비교 함수를 사용하여 정렬된 프로세스 목록 반환
     * @tparam Comparator 비교 함수 타입
     * @param comp 프로세스 정렬에 사용할 비교 함수
     * @return 지정된 비교 함수로 정렬된 프로세스 목록
     */
    template <typename Comparator>
    vector<ProcessInfo> getSortedProcesses(Comparator comp) const;

    /**
     * @brief 프로세스 상태 문자열을 사용자 친화적인 형태로 변환
     * @param status 원본 프로세스 상태 문자열
     * @return 변환된 상태 문자열
     */
    string convertStatus(string_view status) const;

    /**
     * @brief CPU 사용량이 가장 높은 n개 프로세스 반환
     * @param count 반환할 프로세스 수
     * @return CPU 사용량 상위 프로세스 목록
     */
    vector<ProcessInfo> getTopProcessesByCpu(size_t count) const;

    /**
     * @brief 메모리 사용량이 가장 높은 n개 프로세스 반환
     * @param count 반환할 프로세스 수
     * @return 메모리 사용량 상위 프로세스 목록
     */
    vector<ProcessInfo> getTopProcessesByMemory(size_t count) const;

    /**
     * @brief PID 기준 상위 n개 프로세스 반환
     * @param count 반환할 프로세스 수
     * @return PID 기준 상위 프로세스 목록
     */
    vector<ProcessInfo> getTopProcessesByPid(size_t count) const;

    /**
     * @brief 이름 기준 상위 n개 프로세스 반환
     * @param count 반환할 프로세스 수
     * @return 이름 기준 상위 프로세스 목록
     */
    vector<ProcessInfo> getTopProcessesByName(size_t count) const;

public:
    /**
     * @brief 정렬 옵션의 최대 값
     */
    static const int MAX_SORT_BY = 3;

    /**
     * @brief 프로세스 정보 수집 실행
     * @note 이 메서드는 Collector 기본 클래스의 가상 메서드를 오버라이드합니다
     */
    void collect() override;

    /**
     * @brief 지정된 정렬 방식으로 프로세스 목록 반환
     * @param sort_by 정렬 기준 (0: CPU, 1: 메모리, 2: PID, 3: 이름)
     * @return 정렬된 프로세스 목록
     */
    vector<ProcessInfo> getProcesses(int sort_by) const;

    /**
     * @brief 지정된 정렬 방식으로 상위 n개 프로세스 반환
     * @param sort_by 정렬 기준 (0: CPU, 1: 메모리, 2: PID, 3: 이름)
     * @param count 반환할 프로세스 수
     * @return 정렬된 상위 프로세스 목록
     */
    vector<ProcessInfo> getTopProcesses(int sort_by, size_t count) const;

    /**
     * @brief 지정된 PID의 프로세스 종료
     * @param pid 종료할 프로세스의 PID
     * @return 성공 여부 (true: 성공, false: 실패)
     */
    bool killProcess(pid_t pid);
};