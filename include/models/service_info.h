#pragma once

#include <string>
#include <cstdint>

using namespace std;

/**
 * @brief 시스템 서비스의 상태 및 자원 사용량 정보를 저장하는 구조체
 *
 * 이 구조체는 서비스의 기본 정보, 실행 상태 및 자원 사용량(메모리, CPU)을
 * 포함하는 시스템 서비스의 종합적인 정보를 제공합니다.
 */
struct ServiceInfo
{
    /** @brief 서비스의 이름 */
    string name;

    /** @brief 서비스의 현재 상태 (실행 중, 중지됨 등) */
    string status;

    /** @brief 서비스의 활성화 여부 (true: 활성화됨, false: 비활성화됨) */
    bool enabled;

    /** @brief 서비스의 유형 (예: 시스템, 사용자 등) */
    string type;

    /** @brief 서비스의 로드 상태 (예: loaded, not-loaded 등) */
    string load_state;

    /** @brief 서비스의 활성 상태 (예: active, inactive 등) */
    string active_state;

    /** @brief 서비스의 하위 상태 (예: running, dead 등) */
    string sub_state;

    /** @brief 서비스가 사용하는 메모리 크기 (바이트 단위) */
    uint64_t memory_usage;

    /** @brief 서비스가 사용하는 CPU 사용률 (백분율, 0-100) */
    float cpu_usage;
};