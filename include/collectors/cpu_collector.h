#pragma once
#include "collector.h"
#include "models/cpu_info.h"
#include <vector>

using namespace std;

struct stJiffies
{
    long int user = 0;
    long int nice = 0;
    long int system = 0;
    long int idle = 0;
};

class CPUCollector : public Collector
{
private:
    CpuInfo cpuInfo;
    vector<stJiffies> prevCoreJiffies; // 각 코어의 이전 지표
    struct stJiffies curJiffies;
    struct stJiffies prevJiffies;
    unsigned long last_temp_read_time; // 온도 캐싱을 위한 시간 변수 추가
    void collectCpuInfo();

public:
    void collect() override;
    CPUCollector();
    virtual ~CPUCollector();
    const CpuInfo getCpuInfo() const;
};