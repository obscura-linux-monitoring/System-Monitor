#pragma once

#include <string>
#include <map>
#include <vector>

using namespace std;

struct DockerContainerHealth
{
    string status;
    int failing_streak;
    string last_check_output;
};

struct DockerContainerPort
{
    string container_port;
    string host_port;
    string protocol;
};

struct DockerContainerNetwork
{
    string network_name;
    string network_ip;
    string network_mac;
    string network_rx_bytes;
    string network_tx_bytes;
};

struct DockerContainerVolume
{
    string source;
    string destination;
    string mode;
    string type;
};

struct DockerContainerEnv
{
    string key;
    string value;
};

struct DockerContainerLabel
{
    string label_key;
    string label_value;
};

struct DockerContainerInfo
{
    string container_id;
    string container_name;
    string container_image;
    string container_status;
    string container_created;
    DockerContainerHealth container_health;
    vector<DockerContainerPort> container_ports;
    vector<DockerContainerNetwork> container_network;
    vector<DockerContainerVolume> container_volumes;
    vector<DockerContainerEnv> container_env;
    float cpu_usage;
    uint64_t memory_usage;
    uint64_t memory_limit;
    float memory_percent;
    string command;
    uint64_t network_rx_bytes;
    uint64_t network_tx_bytes;
    uint64_t block_read;
    uint64_t block_write;
    int pids;
    int restarts;
    vector<DockerContainerLabel> labels;

    DockerContainerInfo()
        : cpu_usage(0.0f), memory_usage(0), memory_limit(0), memory_percent(0.0f),
          network_rx_bytes(0), network_tx_bytes(0), block_read(0), block_write(0),
          pids(0), restarts(0)
    {
        container_health.failing_streak = 0;
    }
};