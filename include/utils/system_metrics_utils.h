/**
 * @file system_metrics_utils.h
 * @brief 시스템 메트릭 데이터를 다양한 형식으로 변환하는 유틸리티 클래스
 */

#pragma once

#include <string>
#include <nlohmann/json.hpp>

#include "models/system_metrics.h"

using namespace std;

/**
 * @class SystemMetricsUtil
 * @brief 시스템 메트릭 데이터 변환 유틸리티 클래스
 *
 * 시스템 메트릭 데이터를 JSON 형식으로 변환하는 메서드를 제공합니다.
 */
class SystemMetricsUtil
{
public:
    /**
     * @brief SystemMetrics 객체를 JSON 문자열로 변환
     *
     * @param metrics 변환할 SystemMetrics 객체
     * @return string JSON 형식의 문자열
     */
    static string toJson(const SystemMetrics &metrics);
};