/**
 * @file cpu_collector.h
 * @brief CPU 정보 수집 모듈 헤더 파일
 */

#pragma once
#include "collector.h"
#include "models/cpu_info.h"
#include <vector>

using namespace std;

/**
 * @struct stJiffies
 * @brief CPU 사용량 계산을 위한 Jiffies 정보 구조체
 */
struct stJiffies
{
    long int user = 0;   ///< 사용자 프로세스 실행 시간
    long int nice = 0;   ///< 낮은 우선순위 사용자 프로세스 실행 시간
    long int system = 0; ///< 시스템(커널) 프로세스 실행 시간
    long int idle = 0;   ///< 유휴 시간
};

/**
 * @class CPUCollector
 * @brief CPU 정보를 수집하는 Collector 클래스
 * @details CPU 사용량, 온도, 코어 정보 등을 수집하여 저장합니다.
 */
class CPUCollector : public Collector
{
private:
    CpuInfo cpuInfo;                   ///< 수집된 CPU 정보를 저장하는 객체
    vector<stJiffies> prevCoreJiffies; ///< 각 코어의 이전 지표
    struct stJiffies curJiffies;       ///< 현재 CPU 전체 Jiffies 정보
    struct stJiffies prevJiffies;      ///< 이전 CPU 전체 Jiffies 정보
    unsigned long last_temp_read_time; ///< 온도 캐싱을 위한 시간 변수

    /**
     * @brief CPU 관련 정보를 수집하는 함수
     */
    void collectCpuInfo();

public:
    /**
     * @brief CPU 정보를 수집하는 메서드
     * @details CPU 사용량, 온도, 코어 정보 등을 수집합니다.
     */
    void collect() override;

    /**
     * @brief CPUCollector 클래스 생성자
     * @details 멤버 변수 초기화 및 CPU 정보 초기 수집을 수행합니다.
     */
    CPUCollector();

    /**
     * @brief CPUCollector 클래스 소멸자
     */
    virtual ~CPUCollector();

    /**
     * @brief 수집된 CPU 정보를 반환하는 메서드
     * @return CPU 정보 구조체 (CpuInfo)
     */
    const CpuInfo getCpuInfo() const;
};