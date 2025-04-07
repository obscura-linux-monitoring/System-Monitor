/**
 * @file network_collector.cpp
 * @brief 시스템의 네트워크 인터페이스 정보를 수집하는 클래스 구현
 * @details 네트워크 인터페이스의 트래픽 통계, IP 주소, MAC 주소, 상태, 속도, MTU 등의 정보를 수집합니다.
 */

#include "collectors/network_collector.h"
#include "models/network_interface.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>
#include <cstring>

using namespace std;

/**
 * @brief NetworkCollector 클래스의 생성자
 * @details 마지막 수집 시간을 현재 시간으로 초기화합니다.
 */
NetworkCollector::NetworkCollector() : last_collect_time(std::chrono::steady_clock::now()) {}

/**
 * @brief 시스템의 모든 네트워크 인터페이스 정보를 수집합니다.
 * @details /proc/net/dev 파일을 읽어 각 인터페이스의 트래픽 통계를 수집하고,
 *          시스템 호출을 통해 IP 주소, MAC 주소, 상태, 속도, MTU 등의 추가 정보를 얻습니다.
 * @throw runtime_error 소켓 생성이나 /proc/net/dev 파일 열기에 실패한 경우
 */
void NetworkCollector::collect()
{
    // 현재 시간 측정으로 정확한 바이트/초 계산
    auto current_time = std::chrono::steady_clock::now();
    float time_diff = std::chrono::duration<float>(current_time - last_collect_time).count();
    last_collect_time = current_time;

    // 소켓 생성 (인터페이스 정보 얻기 위함)
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        throw runtime_error("Cannot create socket for network interface information");
    }

    // /proc/net/dev에서 트래픽 통계 읽기
    ifstream netdev("/proc/net/dev");
    if (!netdev.is_open())
    {
        close(sock);
        throw runtime_error("Cannot open /proc/net/dev");
    }

    string line;
    // 헤더 2줄 건너뛰기
    getline(netdev, line);
    getline(netdev, line);

    while (getline(netdev, line))
    {
        istringstream iss(line);
        string if_name;
        size_t bytes_recv, packets_recv, errs_recv, drop_recv;
        size_t bytes_sent, packets_sent, errs_sent, drop_sent;

        // 인터페이스 이름 읽기
        getline(iss, if_name, ':');
        if_name.erase(0, if_name.find_first_not_of(" \t"));

        // 수신 통계
        iss >> bytes_recv;   // bytes
        iss >> packets_recv; // packets
        iss >> errs_recv;    // errors
        iss >> drop_recv;    // dropped

        // 다른 수신 통계 4개 건너뛰기 (fifo, frame, compressed, multicast)
        for (int i = 0; i < 4; i++)
        {
            size_t dummy;
            iss >> dummy;
        }

        // 송신 통계
        iss >> bytes_sent;   // bytes
        iss >> packets_sent; // packets
        iss >> errs_sent;    // errors
        iss >> drop_sent;    // dropped

        // IP, MAC, 상태, 속도, MTU 정보 가져오기
        string ip_address = getIPAddress(sock, if_name);
        string mac_address = getMACAddress(if_name);
        string status = getInterfaceStatus(if_name);
        uint64_t speed = getInterfaceSpeed(if_name);
        int mtu = getInterfaceMTU(sock, if_name);

        // 이전 데이터가 있는지 확인
        auto it = interfaces.find(if_name);
        if (it != interfaces.end())
        {
            auto &interface = it->second;

            // 시간 차이가 너무 작으면 나누기 연산 오류 방지
            if (time_diff < 0.001f)
                time_diff = 0.001f;

            // 현재 속도 계산 (bytes/s)
            float current_rx_speed = static_cast<float>(bytes_recv - interface.rx_bytes) / time_diff;
            float current_tx_speed = static_cast<float>(bytes_sent - interface.tx_bytes) / time_diff;

            // 업데이트된 필드 이름에 맞게 수정
            interface.rx_bytes = bytes_recv;
            interface.tx_bytes = bytes_sent;
            interface.rx_bytes_per_sec = current_rx_speed;
            interface.tx_bytes_per_sec = current_tx_speed;
            interface.rx_packets = packets_recv;
            interface.tx_packets = packets_sent;
            interface.rx_errors = errs_recv;
            interface.tx_errors = errs_sent;
            interface.rx_dropped = drop_recv;
            interface.tx_dropped = drop_sent;

            // 추가 정보 업데이트
            interface.ip = ip_address;
            interface.mac = mac_address;
            interface.status = status;
            interface.speed = speed;
            interface.mtu = mtu;
        }
        else
        {
            // 새로운 인터페이스 초기화 (새 구조체에 맞게 수정)
            NetworkInterface new_interface;
            new_interface.interface = if_name;
            new_interface.ip = ip_address;
            new_interface.mac = mac_address;
            new_interface.status = status;
            new_interface.speed = speed;
            new_interface.mtu = mtu;
            new_interface.rx_bytes = bytes_recv;
            new_interface.tx_bytes = bytes_sent;
            new_interface.rx_bytes_per_sec = 0.0f;
            new_interface.tx_bytes_per_sec = 0.0f;
            new_interface.rx_packets = packets_recv;
            new_interface.tx_packets = packets_sent;
            new_interface.rx_errors = errs_recv;
            new_interface.tx_errors = errs_sent;
            new_interface.rx_dropped = drop_recv;
            new_interface.tx_dropped = drop_sent;

            interfaces[if_name] = new_interface;
        }
    }

    close(sock);
}

/**
 * @brief 지정된 네트워크 인터페이스의 IP 주소를 반환합니다.
 * @param sock 시스템 호출에 사용할 소켓 디스크립터
 * @param if_name 네트워크 인터페이스 이름
 * @return 인터페이스의 IP 주소 (문자열), 할당된 IP가 없거나 오류 시 빈 문자열 반환
 */
string NetworkCollector::getIPAddress(int sock, const string &if_name)
{
    struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
    {
        return ""; // 인터페이스에 IP가 할당되지 않았거나 오류 발생
    }

    return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}

/**
 * @brief 지정된 네트워크 인터페이스의 MAC 주소를 반환합니다.
 * @param if_name 네트워크 인터페이스 이름
 * @return 인터페이스의 MAC 주소 (문자열), 오류 시 빈 문자열 반환
 */
string NetworkCollector::getMACAddress(const string &if_name)
{
    string mac_path = "/sys/class/net/" + if_name + "/address";
    ifstream mac_file(mac_path);

    if (!mac_file.is_open())
    {
        return "";
    }

    string mac;
    getline(mac_file, mac);
    return mac;
}

/**
 * @brief 지정된 네트워크 인터페이스의 상태를 반환합니다.
 * @param if_name 네트워크 인터페이스 이름
 * @return 인터페이스의 상태 (문자열, 일반적으로 "up" 또는 "down"), 오류 시 "unknown" 반환
 */
string NetworkCollector::getInterfaceStatus(const string &if_name)
{
    string operstate_path = "/sys/class/net/" + if_name + "/operstate";
    ifstream operstate_file(operstate_path);

    if (!operstate_file.is_open())
    {
        return "unknown";
    }

    string status;
    getline(operstate_file, status);
    return status;
}

/**
 * @brief 지정된 네트워크 인터페이스의 속도를 반환합니다.
 * @param if_name 네트워크 인터페이스 이름
 * @return 인터페이스의 속도 (Mbps), 오류 시 0 반환
 */
uint64_t NetworkCollector::getInterfaceSpeed(const string &if_name)
{
    string speed_path = "/sys/class/net/" + if_name + "/speed";
    ifstream speed_file(speed_path);

    if (!speed_file.is_open())
    {
        return 0;
    }

    // 먼저 int로 읽어서 유효성 검사
    int temp_speed = 0;
    if (!(speed_file >> temp_speed) || temp_speed < 0)
    {
        return 0;
    }

    return static_cast<uint64_t>(temp_speed);
}

/**
 * @brief 지정된 네트워크 인터페이스의 MTU(Maximum Transmission Unit)를 반환합니다.
 * @param sock 시스템 호출에 사용할 소켓 디스크립터
 * @param if_name 네트워크 인터페이스 이름
 * @return 인터페이스의 MTU 값, 오류 시 0 반환
 */
int NetworkCollector::getInterfaceMTU(int sock, const string &if_name)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFMTU, &ifr) < 0)
    {
        return 0;
    }

    return ifr.ifr_mtu;
}

/**
 * @brief 수집된 모든 네트워크 인터페이스 정보를 맵 형태로 반환합니다.
 * @return 인터페이스 이름을 키로, NetworkInterface 객체를 값으로 하는 맵
 */
map<string, NetworkInterface> NetworkCollector::getInterfaces() const
{
    return interfaces;
}

/**
 * @brief 수집된 모든 네트워크 인터페이스 정보를 벡터 형태로 반환합니다.
 * @details 내부 맵에 저장된 인터페이스 정보를 벡터로 변환하여 반환합니다.
 * @return NetworkInterface 객체들의 벡터
 */
vector<NetworkInterface> NetworkCollector::getInterfacesToVector() const
{
    vector<NetworkInterface> result;
    result.reserve(interfaces.size()); // 메모리 미리 할당으로 효율성 개선

    for (const auto &pair : interfaces)
    {
        result.push_back(pair.second);
    }
    return result;
}