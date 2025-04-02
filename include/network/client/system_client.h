/**
 * @file system_client.h
 * @brief 시스템 정보를 수집하고 서버로 전송하는 클라이언트 클래스 정의
 * @author System Monitor Team
 */

#pragma once

#include "network/common/network_types.h"
#include "models/system_metrics.h"

#include <string>
#include <memory>

using namespace std;

/**
 * @brief 전방 선언
 */
class CollectorManager;
class DataSender;

/**
 * @brief 시스템 모니터링 클라이언트 클래스
 *
 * 시스템 메트릭을 수집하고 지정된 서버로 전송하는 기능을 제공합니다.
 * 설정된 간격으로 시스템 정보를 수집하고 서버에 전송합니다.
 */
class SystemClient
{
public:
    /**
     * @brief SystemClient 클래스의 생성자
     *
     * @param serverInfo 연결할 서버 정보 (주소, 포트 등)
     * @param systemKey 시스템 식별을 위한 고유 키
     * @param collectionInterval 데이터 수집 간격 (초 단위, 기본값: 5초)
     * @param sendingInterval 데이터 전송 간격 (초 단위, 기본값: 5초)
     * @param user_id 사용자 식별자 (기본값: 빈 문자열)
     */
    SystemClient(const ServerInfo &serverInfo, const string &systemKey,
                 int collectionInterval = 5, int sendingInterval = 5, const string &user_id = "");

    /**
     * @brief SystemClient 클래스의 소멸자
     *
     * 리소스 해제 및 연결 종료를 처리합니다.
     */
    ~SystemClient();

    /**
     * @brief 서버에 연결하고 데이터 수집/전송 시작
     *
     * 서버에 연결하고 설정된 간격으로 데이터 수집 및 전송을 시작합니다.
     */
    void connect();

    /**
     * @brief 모든 작업 중지 및 연결 종료
     *
     * 데이터 수집 및 전송을 중지하고 서버와의 연결을 종료합니다.
     */
    void disconnect();

    /**
     * @brief 연결 상태 확인
     *
     * @return 서버에 연결되어 있으면 true, 그렇지 않으면 false
     */
    bool isConnected() const;

private:
    /**
     * @brief 서버 연결 정보
     */
    ServerInfo serverInfo_;

    /**
     * @brief 시스템 식별 키
     */
    string systemKey_;

    /**
     * @brief 사용자 식별자
     */
    string user_id_;

    /**
     * @brief 시스템 메트릭 수집 관리자
     */
    unique_ptr<CollectorManager> collectorManager_;

    /**
     * @brief 수집된 데이터 전송 객체
     */
    unique_ptr<DataSender> dataSender_;

    /**
     * @brief 데이터 수집 간격 (초)
     */
    int collectionInterval_;

    /**
     * @brief 데이터 전송 간격 (초)
     */
    int sendingInterval_;
};