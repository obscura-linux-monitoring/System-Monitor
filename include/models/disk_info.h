#pragma once

#include <string>

using namespace std;

struct IoStats
{
    size_t reads;
    size_t writes;
    size_t read_bytes;
    size_t write_bytes;
    time_t read_time;
    time_t write_time;
    time_t io_time;
    size_t io_in_progress;
    double reads_per_sec;
    double writes_per_sec;
    double read_bytes_per_sec;
    double write_bytes_per_sec;
    bool error_flag;

    IoStats() : reads(0), writes(0), read_bytes(0), write_bytes(0),
               read_time(0), write_time(0), io_time(0), io_in_progress(0),
               reads_per_sec(0), writes_per_sec(0), 
               read_bytes_per_sec(0), write_bytes_per_sec(0),
               error_flag(false) {}
};

struct DiskInfo
{
    string device;
    string mount_point;
    string filesystem_type;
    size_t total;
    size_t used;
    size_t free;
    float usage_percent;
    size_t inodes_total;
    size_t inodes_used;
    size_t inodes_free;
    IoStats io_stats;
    bool error_flag;
    string error_message;
};