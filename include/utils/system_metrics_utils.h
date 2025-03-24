#pragma once

#include <string>
#include <nlohmann/json.hpp>

#include "models/system_metrics.h"

using namespace std;

class SystemMetricsUtil
{
public:
    static string toJson(const SystemMetrics &metrics);
};