#pragma once

#include <cstdint>

struct MemoryInfo
{
    uint64_t total;
    uint64_t used;
    uint64_t free;
    uint64_t cached;
    uint64_t buffers;
    uint64_t available;
    uint64_t swap_total;
    uint64_t swap_used;
    uint64_t swap_free;
    float usage_percent;
};