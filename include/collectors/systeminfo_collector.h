#pragma once
#include "collector.h"
#include "models/system_info.h"
#include <string>

using namespace std;

/**
 * @class SystemInfoCollector
 * @brief 시스템 정보를 수집하는 컬렉터 클래스
 *
 * 이 클래스는 Collector 클래스를 상속받아 호스트명, 운영체제 정보,
 * 커널 버전, 아키텍처 등 시스템의 기본 정보를 수집합니다.
 *
 * @see Collector
 * @see SystemInfo
 */
class SystemInfoCollector : public Collector
{
private:
    /**
     * @var system_info
     * @brief 수집된 시스템 정보를 저장하는 객체
     *
     * 호스트명, 운영체제 정보, 버전, 아키텍처, 가동 시간 등의
     * 시스템 정보를 저장합니다.
     */
    SystemInfo system_info;

public:
    /**
     * @brief 기본 생성자
     *
     * SystemInfoCollector 객체를 초기화하고 system_info 객체를 기본값으로 설정합니다.
     */
    SystemInfoCollector();

    /**
     * @brief 시스템 정보 수집 메서드
     *
     * 호스트명, 운영체제 정보, 커널 버전, 아키텍처, 시스템 가동 시간 등의
     * 정보를 수집하여 system_info 객체에 저장합니다.
     *
     * @note Collector 클래스의 순수 가상 함수를 구현합니다.
     */
    void collect() override;

    /**
     * @brief 가상 소멸자
     *
     * 클래스의 리소스를 정리합니다.
     */
    virtual ~SystemInfoCollector();

    /**
     * @brief 수집된 시스템 정보 반환
     *
     * @return const SystemInfo& 수집된 시스템 정보에 대한 상수 참조
     */
    const SystemInfo &getSystemInfo() const;
};