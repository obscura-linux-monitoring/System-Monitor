#pragma once
#include "collector.h"
#include "models/memory_info.h"

class MemoryCollector : public Collector
{
private:
    MemoryInfo memoryInfo;
    void clear();

public:
    void collect() override;
    MemoryInfo getMemoryInfo() const;
};