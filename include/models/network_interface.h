#pragma once

#include <string>
#include <cstdint>

using namespace std;

struct NetworkInterface
{
    string interface;
    string ip;
    string mac;
    string status;
    uint64_t speed;
    uint64_t mtu;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
    float rx_bytes_per_sec;
    float tx_bytes_per_sec;
};