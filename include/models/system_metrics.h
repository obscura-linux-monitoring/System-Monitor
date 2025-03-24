#pragma once

#include <string>
#include <vector>

#include "models/cpu_info.h"
#include "models/memory_info.h"
#include "models/disk_info.h"
#include "models/network_interface.h"
#include "models/process_info.h"
#include "models/docker_container_info.h"
#include "models/system_info.h"
#include "models/service_info.h"

using namespace std;

struct SystemMetrics
{
    string key;
    string timestamp;
    SystemInfo system;
    CpuInfo cpu;
    MemoryInfo memory;
    vector<DiskInfo> disk;
    vector<NetworkInterface> network;
    vector<ProcessInfo> process;
    vector<DockerContainerInfo> docker;
    vector<ServiceInfo> services;
};