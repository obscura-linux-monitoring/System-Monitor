#pragma once

#include "network/common/network_types.h"
#include "models/system_metrics.h"

#include <string>
#include <memory>

// 전방 선언
class CollectorManager;
class DataSender;

class SystemClient
{
public:
    SystemClient(const ServerInfo &serverInfo, const std::string &systemKey,
                 int collectionInterval = 5, int sendingInterval = 5);
    ~SystemClient();

    // 서버에 연결하고 데이터 수집/전송 시작
    void connect();

    // 모든 작업 중지 및 연결 종료
    void disconnect();

    // 연결 상태 확인
    bool isConnected() const;

private:
    ServerInfo serverInfo_;
    std::string systemKey_;

    // 수집 관리자
    std::unique_ptr<CollectorManager> collectorManager_;

    // 데이터 송신기
    std::unique_ptr<DataSender> dataSender_;

    // 수집 간격 (초)
    int collectionInterval_;

    // 전송 간격 (초)
    int sendingInterval_;
};