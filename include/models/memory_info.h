/**
 * @file memory_info.h
 * @brief 시스템 메모리 정보를 저장하는 구조체 정의
 * @author System Monitor Team
 */
#pragma once

#include <cstdint>

/**
 * @brief 시스템 메모리 정보를 저장하는 구조체
 *
 * 물리적 메모리와 스왑 메모리에 관한 상세 정보를 포함합니다.
 */
struct MemoryInfo
{
    uint64_t total;      ///< 전체 물리 메모리 크기 (바이트)
    uint64_t used;       ///< 사용 중인 물리 메모리 (바이트)
    uint64_t free;       ///< 사용 가능한 물리 메모리 (바이트)
    uint64_t cached;     ///< 캐시로 사용 중인 메모리 (바이트)
    uint64_t buffers;    ///< 버퍼로 사용 중인 메모리 (바이트)
    uint64_t available;  ///< 실제 사용 가능한 메모리 (바이트)
    uint64_t swap_total; ///< 전체 스왑 메모리 크기 (바이트)
    uint64_t swap_used;  ///< 사용 중인 스왑 메모리 (바이트)
    uint64_t swap_free;  ///< 사용 가능한 스왑 메모리 (바이트)
    float usage_percent; ///< 메모리 사용률 (%)
};