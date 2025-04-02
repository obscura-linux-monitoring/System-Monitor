/**
 * @file memory_collector.h
 * @brief 시스템 메모리 정보를 수집하는 클래스 정의
 *
 * 이 파일은 시스템의 메모리 사용량 정보를 수집하고 제공하는
 * MemoryCollector 클래스를 정의합니다.
 */

#pragma once
#include "collector.h"
#include "models/memory_info.h"

/**
 * @class MemoryCollector
 * @brief 시스템 메모리 정보를 수집하는 클래스
 *
 * Linux 시스템의 메모리 정보(/proc/meminfo)를 읽어 총 메모리, 사용 중인 메모리,
 * 여유 메모리, 캐시, 버퍼, 스왑 메모리 등의 정보를 수집합니다.
 *
 * @see Collector
 * @see MemoryInfo
 */
class MemoryCollector : public Collector
{
private:
    /**
     * @brief 수집된 메모리 정보를 저장하는 객체
     */
    MemoryInfo memoryInfo;

    /**
     * @brief 메모리 정보를 초기화하는 내부 메서드
     *
     * 모든 메모리 관련 값을 0으로 초기화합니다.
     */
    void clear();

public:
    /**
     * @brief 시스템 메모리 정보를 수집하는 메서드
     *
     * /proc/meminfo 파일을 읽어 메모리 정보를 수집하고
     * 내부 memoryInfo 객체에 저장합니다.
     *
     * @throws runtime_error /proc/meminfo 파일을 열 수 없는 경우
     */
    void collect() override;

    /**
     * @brief 수집된 메모리 정보를 반환하는 메서드
     *
     * @return MemoryInfo 수집된 메모리 정보 객체
     */
    MemoryInfo getMemoryInfo() const;
};