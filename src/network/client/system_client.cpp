#include "network/client/system_client.h"
#include "collectors/collector_manager.h"
#include "network/client/data_sender.h"
#include "common/thread_safe_queue.h"
#include "models/system_metrics.h"
#include "utils/system_metrics_utils.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace std;

SystemClient::SystemClient(const ServerInfo &serverInfo, const string &systemKey,
                           int collectionInterval, int sendingInterval)
    : serverInfo_(serverInfo), systemKey_(systemKey),
      collectionInterval_(collectionInterval), sendingInterval_(sendingInterval)
{
    // 시작 시간 기록
    auto startTime = chrono::system_clock::now();
    time_t start_time_t = chrono::system_clock::to_time_t(startTime);
    char startTimestamp[30];
    strftime(startTimestamp, sizeof(startTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&start_time_t));

    cout << "[시스템] 초기화 시작: " << startTimestamp << endl;

    // 수집 관리자 초기화
    collectorManager_ = make_unique<CollectorManager>(systemKey_);

    // 데이터 송신기 초기화
    dataSender_ = make_unique<DataSender>(serverInfo_, collectorManager_->getDataQueue());

    // 종료 시간 기록 및 소요 시간 출력
    auto endTime = chrono::system_clock::now();
    time_t end_time_t = chrono::system_clock::to_time_t(endTime);
    char endTimestamp[30];
    strftime(endTimestamp, sizeof(endTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&end_time_t));

    auto initDuration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
    cout << "[시스템] 초기화 완료: " << endTimestamp
         << ", 소요 시간: " << initDuration.count() << "ms" << endl;
}

SystemClient::~SystemClient()
{
    disconnect();
}

void SystemClient::connect()
{
    // 시작 시간 기록
    auto connectStartTime = chrono::system_clock::now();
    time_t start_time_t = chrono::system_clock::to_time_t(connectStartTime);
    char startTimestamp[30];
    strftime(startTimestamp, sizeof(startTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&start_time_t));

    cout << "[시스템] 연결 시작: " << startTimestamp << endl;

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
    cout << "[시스템] 연결 " << (connected ? "성공" : "실패") << ": " << endTimestamp
         << ", 소요 시간: " << connectDuration.count() << "ms" << endl;
}

void SystemClient::disconnect()
{
    // 수집 중지
    if (collectorManager_)
    {
        collectorManager_->stop();
    }

    // 송신 중지 및 연결 해제
    if (dataSender_)
    {
        dataSender_->stopSending();
        dataSender_->disconnect();
    }
}

bool SystemClient::isConnected() const
{
    return dataSender_ && dataSender_->isConnected();
}