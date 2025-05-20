#pragma once

#include <string>
#include <cstdint>

using namespace std;

/**
 * @brief 네트워크 인터페이스 정보 및 통계를 저장하는 구조체
 *
 * 이 구조체는 네트워크 인터페이스의 기본 정보(이름, IP, MAC 주소, 상태)와
 * 성능 관련 속성(속도, MTU) 및 트래픽 통계(바이트, 패킷, 오류, 드롭 수)를
 * 저장합니다.
 */
struct NetworkInterface
{
    string interface;       /**< 네트워크 인터페이스 이름 (예: eth0, wlan0) */
    string ipv4;            /**< 인터페이스의 IPv4 주소 */
    string ipv6;            /**< 인터페이스의 IPv6 주소 */
    string mac;             /**< 인터페이스의 MAC(물리적) 주소 */
    string status;          /**< 인터페이스의 현재 상태 (예: up, down) */
    uint64_t speed;         /**< 인터페이스의 속도 (Mbps) */
    int mtu;                /**< 최대 전송 단위 (Maximum Transmission Unit) */
    uint64_t rx_bytes;      /**< 수신된 총 바이트 수 */
    uint64_t tx_bytes;      /**< 전송된 총 바이트 수 */
    uint64_t rx_packets;    /**< 수신된 총 패킷 수 */
    uint64_t tx_packets;    /**< 전송된 총 패킷 수 */
    uint64_t rx_errors;     /**< 수신 중 발생한 오류 수 */
    uint64_t tx_errors;     /**< 전송 중 발생한 오류 수 */
    uint64_t rx_dropped;    /**< 수신 중 드롭된 패킷 수 */
    uint64_t tx_dropped;    /**< 전송 중 드롭된 패킷 수 */
    float rx_bytes_per_sec; /**< 초당 수신 바이트 수 (현재 대역폭 사용률) */
    float tx_bytes_per_sec; /**< 초당 전송 바이트 수 (현재 대역폭 사용률) */
    string connection_type; /**< 인터페이스의 연결 타입 (예: ethernet, wifi) */
};