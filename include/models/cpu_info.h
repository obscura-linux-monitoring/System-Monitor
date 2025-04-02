/**
 * @file cpu_info.h
 * @brief CPU 정보 및 코어 정보를 정의하는 구조체
 * @author System Monitor Team
 */

#pragma once

#include <string>
#include <vector>

using namespace std;

/**
 * @struct CpuCoreInfo
 * @brief CPU 코어에 대한 정보를 저장하는 구조체
 */
struct CpuCoreInfo
{
    int id;            ///< 코어 식별자
    float usage;       ///< 코어 사용률 (백분율)
    float temperature; ///< 코어 온도 (섭씨)
};

/**
 * @struct CpuInfo
 * @brief CPU에 대한 전반적인 정보를 저장하는 구조체
 */
struct CpuInfo
{
    string model;               ///< CPU 모델명
    string vendor;              ///< CPU 제조사
    string architecture;        ///< CPU 아키텍처 (x86, x86_64, ARM 등)
    float usage;                ///< 전체 CPU 사용률 (백분율)
    float temperature;          ///< 전체 CPU 온도 (섭씨)
    size_t total_cores;         ///< 물리적 코어 총 개수
    size_t total_logical_cores; ///< 논리적 코어 총 개수
    bool is_hyperthreading;     ///< 하이퍼스레딩 지원 여부
    float clock_speed;          ///< CPU 클럭 속도 (GHz)
    int cache_size;             ///< CPU 캐시 크기 (KB)
    vector<CpuCoreInfo> cores;  ///< 개별 코어 정보 배열
    bool has_vmx;               ///< Intel VT 지원 여부
    bool has_svm;               ///< AMD-V 지원 여부
    bool has_avx;               ///< AVX 지원 여부
    bool has_avx2;              ///< AVX2 지원 여부
    bool has_neon;              ///< NEON 지원 여부
    bool has_sve;               ///< SVE 지원 여부
};