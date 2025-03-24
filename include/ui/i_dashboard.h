#pragma once
#include "collectors/cpu_collector.h"
#include "collectors/memory_collector.h"
#include "collectors/disk_collector.h"
#include "collectors/network_collector.h"

class IDashboard
{
public:
    virtual ~IDashboard() = default;
    virtual void init() = 0;
    virtual void update() = 0;
    virtual void handleInput() = 0;
    virtual void cleanup() = 0;
};