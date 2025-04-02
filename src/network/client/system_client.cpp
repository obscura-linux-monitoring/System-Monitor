/**
 * @file system_client.cpp
 * @brief 시스템 클라이언트 구현
 * @details 서버와의 통신을 관리하고 시스템 메트릭을 수집하여 전송하는 클라이언트 클래스 구현
 */

#include "network/client/system_client.h"
#include "collectors/collector_manager.h"
#include "network/client/data_sender.h"
#include "common/thread_safe_queue.h"
#include "models/system_metrics.h"
#include "utils/system_metrics_utils.h"
#include "log/logger.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace std;

bool isDisconnected_ = false;

/**
 * @brief SystemClient 클래스의 생성자
 * 
 * @param serverInfo 서버 연결 정보
 * @param systemKey 시스템 식별 키
 * @param collectionInterval 메트릭 수집 간격(초)
 * @param sendingInterval 데이터 전송 간격(초)
 * @param user_id 사용자 ID
 * 
 * @details 시스템 클라이언트를 초기화하고 CollectorManager와 DataSender를 설정합니다.
 */
SystemClient::SystemClient(const ServerInfo &serverInfo, const string &systemKey,
                           int collectionInterval, int sendingInterval, const string &user_id)
    : serverInfo_(serverInfo), systemKey_(systemKey),
      collectionInterval_(collectionInterval), sendingInterval_(sendingInterval), user_id_(user_id)
{
    // 시작 시간 기록
    auto startTime = chrono::system_clock::now();
    time_t start_time_t = chrono::system_clock::to_time_t(startTime);
    char startTimestamp[30];
    strftime(startTimestamp, sizeof(startTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&start_time_t));

    LOG_INFO("시스템 클라이언트 초기화 시작: {}", startTimestamp);

    // 수집 관리자 초기화
    collectorManager_ = make_unique<CollectorManager>(systemKey_);

    // 데이터 송신기 초기화
    dataSender_ = make_unique<DataSender>(serverInfo_, collectorManager_->getDataQueue(), user_id_);

    // 종료 시간 기록 및 소요 시간 출력
    auto endTime = chrono::system_clock::now();
    time_t end_time_t = chrono::system_clock::to_time_t(endTime);
    char endTimestamp[30];
    strftime(endTimestamp, sizeof(endTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&end_time_t));

    auto initDuration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
    LOG_INFO("시스템 클라이언트 초기화 완료: {}, 소요 시간: {}ms", endTimestamp, initDuration.count());
}

/**
 * @brief SystemClient 클래스의 소멸자
 * 
 * @details 클라이언트 연결을 안전하게 종료하고 리소스를 정리합니다.
 */
SystemClient::~SystemClient()
{
    disconnect();
}

/**
 * @brief 서버에 연결하고 데이터 수집 및 전송을 시작
 * 
 * @details 
 * 1. 수집 관리자를 시작하여 메트릭 수집 시작
 * 2. 데이터 송신기를 통해 서버에 연결
 * 3. 정기적인 데이터 전송 시작
 * 
 * @return void
 */
void SystemClient::connect()
{
    // 시작 시간 기록
    auto connectStartTime = chrono::system_clock::now();
    time_t start_time_t = chrono::system_clock::to_time_t(connectStartTime);
    char startTimestamp[30];
    strftime(startTimestamp, sizeof(startTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&start_time_t));

    LOG_INFO("시스템 클라이언트 연결 시작: {}", startTimestamp);

    // 수집 시작
    collectorManager_->start(collectionInterval_);

    // 서버 연결 및 송신 시작
    bool connected = false;
    if (dataSender_->connect())
    {
        dataSender_->startSending(sendingInterval_);
        connected = true;
    }

    // 종료 시간 기록 및 소요 시간 출력
    auto connectEndTime = chrono::system_clock::now();
    time_t end_time_t = chrono::system_clock::to_time_t(connectEndTime);
    char endTimestamp[30];
    strftime(endTimestamp, sizeof(endTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&end_time_t));

    auto connectDuration = chrono::duration_cast<chrono::milliseconds>(connectEndTime - connectStartTime);
    LOG_INFO("시스템 클라이언트 연결 {} : {}, 소요 시간: {}ms", (connected ? "성공" : "실패"), endTimestamp, connectDuration.count());
}

/**
 * @brief 서버 연결을 종료하고 데이터 수집 및 전송을 중지
 * 
 * @details 다음 순서로 안전하게 종료합니다:
 * 1. 데이터 송신 중지
 * 2. 메트릭 수집 중지
 * 3. 서버 연결 해제
 * 
 * @return void
 */
void SystemClient::disconnect()
{
    if (isDisconnected_)
        return;

    isDisconnected_ = true;

    // 먼저 데이터 송신을 중지
    if (dataSender_)
    {
        dataSender_->stopSending();
    }

    // 그 다음 수집을 중지
    if (collectorManager_)
    {
        collectorManager_->stop();
    }

    // 마지막으로 연결 해제
    if (dataSender_)
    {
        dataSender_->disconnect();
    }

    LOG_INFO("시스템 클라이언트가 정상적으로 종료되었습니다.");
}

/**
 * @brief 서버 연결 상태 확인
 * 
 * @return bool 서버에 연결되어 있으면 true, 아니면 false
 */
bool SystemClient::isConnected() const
{
    return dataSender_ && dataSender_->isConnected();
}