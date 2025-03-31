#pragma once

#include "network/common/network_types.h"
#include "models/system_metrics.h"
#include "common/thread_safe_queue.h"
#include "utils/system_metrics_utils.h"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>

using namespace std;

class DataSender
{
public:
    DataSender(const ServerInfo &serverInfo, ThreadSafeQueue<SystemMetrics> &dataQueue, const string &user_id);
    ~DataSender();

    // 서버 연결
    bool connect();

    // 연결 종료
    void disconnect();

    // 데이터 전송 시작
    void startSending(int intervalSeconds = 10);

    // 데이터 전송 중지
    void stopSending();

    // 연결 상태 확인
    bool isConnected() const { return isConnected_; }

private:
    typedef websocketpp::client<websocketpp::config::asio_client> WebsocketClient;
    typedef websocketpp::connection_hdl WebsocketHandle;

    ServerInfo serverInfo_;
    ThreadSafeQueue<SystemMetrics> &dataQueue_;
    string user_id_;

    WebsocketClient client_;
    WebsocketHandle connectionHandle_;
    atomic<bool> isConnected_;
    atomic<bool> running_;

    thread clientThread_; // WebSocket 통신 스레드
    thread senderThread_; // 데이터 송신 스레드

    // 데이터 전송 루프
    void sendLoop(int intervalSeconds);

    // 단일 데이터 전송
    bool sendMetrics(const SystemMetrics &metrics);

    // 메시지 수신 처리
    void handleMessage(WebsocketHandle hdl, WebsocketClient::message_ptr msg);
};
