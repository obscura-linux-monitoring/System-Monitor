#pragma once

#include <string>
#include <cstdint>

using namespace std;

struct ServiceInfo
{
    string name;
    string status;
    bool enabled;
    string type;
    string load_state;
    string active_state;
    string sub_state;
    uint64_t memory_usage;
    float cpu_usage;
};