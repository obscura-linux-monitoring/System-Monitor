/**
 * @file network_types.h
 * @brief 네트워크 통신에 필요한 기본 데이터 타입 정의
 * @author 시스템 모니터 개발팀
 */
#pragma once

#include <string>
#include <cstdint>

using namespace std;

/**
 * @brief 서버 연결 정보를 저장하는 구조체
 *
 * 이 구조체는 서버의 주소와 포트 번호를 포함하여
 * 네트워크 연결을 설정하는 데 필요한 정보를 제공합니다.
 */
struct ServerInfo
{
    /**
     * @brief 서버의 호스트 주소 또는 IP 주소
     */
    string address;

    /**
     * @brief 서버의 포트 번호
     */
    uint16_t port;
};