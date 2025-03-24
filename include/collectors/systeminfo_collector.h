#pragma once
#include "collector.h"
#include "models/system_info.h"
#include <string>

using namespace std;

class SystemInfoCollector : public Collector
{
private:
    SystemInfo system_info;

public:
    SystemInfoCollector(); // 생성자 선언 추가
    void collect() override;
    virtual ~SystemInfoCollector();
    const SystemInfo &getSystemInfo() const;
};