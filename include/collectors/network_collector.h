#pragma once

#include <map>
#include <string>
#include <vector>
#include <chrono>
#include "models/network_interface.h"

using namespace std;

/**
 * @class NetworkCollector
 * @brief 시스템의 네트워크 인터페이스 정보를 수집하고 관리하는 클래스
 *
 * 이 클래스는 시스템의 모든 네트워크 인터페이스에 대한 정보(IP, MAC, 속도, 트래픽 등)를
 * 수집하고 저장합니다. /proc/net/dev 및 시스템 API를 사용하여 정보를 수집합니다.
 */
class NetworkCollector
{
private:
    /**
     * @brief 네트워크 인터페이스 정보를 저장하는 맵
     *
     * 인터페이스 이름을 키로 사용하여 해당 인터페이스의 정보를 저장합니다.
     */
    map<string, NetworkInterface> interfaces;

    /**
     * @brief 마지막 정보 수집 시간
     *
     * 네트워크 속도(초당 바이트 수) 계산을 위해 사용됩니다.
     */
    chrono::steady_clock::time_point last_collect_time;

    /**
     * @brief 인터페이스의 IPv4 주소를 얻는 함수
     *
     * @param sock 네트워크 소켓 디스크립터
     * @param if_name 인터페이스 이름
     * @return IP 주소 문자열, 실패 시 빈 문자열 반환
     */
    string getIPv4Address(int sock, const string &if_name);

    /**
     * @brief 인터페이스의 IPv6 주소를 얻는 함수
     *
     * @param sock 네트워크 소켓 디스크립터
     * @param if_name 인터페이스 이름
     * @return IPv6 주소 문자열, 실패 시 빈 문자열 반환
     */
    string getIPv6Address(int sock, const string &if_name);
    /**
     * @brief 인터페이스의 MAC 주소를 얻는 함수
     *
     * @param if_name 인터페이스 이름
     * @return MAC 주소 문자열, 실패 시 빈 문자열 반환
     */
    string getMACAddress(const string &if_name);

    /**
     * @brief 인터페이스의 작동 상태를 얻는 함수
     *
     * @param if_name 인터페이스 이름
     * @return 인터페이스 상태 (up, down 등)
     */
    string getInterfaceStatus(const string &if_name);

    /**
     * @brief 인터페이스의 속도(Mbps)를 얻는 함수
     *
     * @param if_name 인터페이스 이름
     * @return 인터페이스 속도, 실패 시 0 반환
     */
    uint64_t getInterfaceSpeed(const string &if_name);

    /**
     * @brief 인터페이스의 MTU(Maximum Transmission Unit)를 얻는 함수
     *
     * @param sock 네트워크 소켓 디스크립터
     * @param if_name 인터페이스 이름
     * @return MTU 값, 실패 시 0 반환
     */
    int getInterfaceMTU(int sock, const string &if_name);

    /**
     * @brief 인터페이스의 연결 타입을 얻는 함수
     *
     * @param if_name 인터페이스 이름
     * @return 인터페이스 연결 타입 (예: ethernet, wifi)
     */
    string getConnectionType(const string &if_name);

public:
    /**
     * @brief 생성자
     *
     * 초기 수집 시간을 현재 시간으로 초기화합니다.
     */
    NetworkCollector();

    /**
     * @brief 네트워크 인터페이스 정보 수집 함수
     *
     * 모든 네트워크 인터페이스에 대한 정보를 수집하고 저장합니다.
     * 인터페이스 속도, 패킷 수, 에러 수 등의 통계를 업데이트합니다.
     */
    void collect();

    /**
     * @brief 저장된 인터페이스 정보를 맵 형태로 반환
     *
     * @return 인터페이스 이름을 키로 하는 NetworkInterface 객체 맵
     */
    map<string, NetworkInterface> getInterfaces() const;

    /**
     * @brief 저장된 인터페이스 정보를 벡터 형태로 반환
     *
     * @return NetworkInterface 객체의 벡터
     */
    vector<NetworkInterface> getInterfacesToVector() const;
};