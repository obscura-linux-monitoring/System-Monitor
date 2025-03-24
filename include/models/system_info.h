#pragma once

#include <string>
#include <ctime>

using namespace std;

struct SystemInfo
{
    string hostname;
    string os_name;
    string os_version;
    string os_kernel_version;
    string os_architecture;
    time_t uptime;
    time_t boot_time;
};