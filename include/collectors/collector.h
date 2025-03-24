#pragma once

class Collector {
public:
    virtual void collect() = 0;
    virtual ~Collector() = default;
}; 