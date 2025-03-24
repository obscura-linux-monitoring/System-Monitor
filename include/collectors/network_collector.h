#pragma once

#include <map>
#include <string>
#include <vector>
#include <chrono>
#include "models/network_interface.h"

using namespace std;

class NetworkCollector
{
private:
    map<string, NetworkInterface> interfaces;
    chrono::steady_clock::time_point last_collect_time;

    // 네트워크 인터페이스 정보 수집 도우미 함수들
    string getIPAddress(int sock, const string &if_name);
    string getMACAddress(const string &if_name);
    string getInterfaceStatus(const string &if_name);
    int getInterfaceSpeed(const string &if_name);
    int getInterfaceMTU(int sock, const string &if_name);

public:
    NetworkCollector();
    void collect();
    map<string, NetworkInterface> getInterfaces() const;
    vector<NetworkInterface> getInterfacesToVector() const;
};