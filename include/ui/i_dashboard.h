#pragma once
#include "collectors/cpu_collector.h"
#include "collectors/memory_collector.h"
#include "collectors/disk_collector.h"
#include "collectors/network_collector.h"

/**
 * @brief 시스템 모니터링 대시보드의 인터페이스
 *
 * 시스템 리소스 정보(CPU, 메모리, 디스크, 네트워크)를 수집하고
 * 사용자에게 시각적으로 표시하는 대시보드의 기본 인터페이스를 정의합니다.
 */
class IDashboard
{
public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~IDashboard() = default;

    /**
     * @brief 대시보드 초기화
     *
     * 대시보드 컴포넌트와 필요한 리소스를 초기화합니다.
     */
    virtual void init() = 0;

    /**
     * @brief 대시보드 업데이트
     *
     * 최신 시스템 정보를 수집하고 대시보드 표시를 업데이트합니다.
     */
    virtual void update() = 0;

    /**
     * @brief 사용자 입력 처리
     *
     * 키보드, 마우스 등의 사용자 입력을 처리합니다.
     */
    virtual void handleInput() = 0;

    /**
     * @brief 대시보드 정리
     *
     * 사용한 리소스 해제 및 프로그램 종료 전 정리 작업을 수행합니다.
     */
    virtual void cleanup() = 0;
};