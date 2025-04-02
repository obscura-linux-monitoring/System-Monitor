#pragma once

#include "network/common/network_types.h"
#include "models/system_metrics.h"
#include "common/thread_safe_queue.h"
#include "utils/system_metrics_utils.h"
#include "models/command_result.h"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

using namespace std;

/**
 * @file data_sender.h
 * @brief 시스템 메트릭 데이터를 서버로 전송하기 위한 클래스를 정의합니다.
 * @author System Monitor Team
 */

/**
 * @class DataSender
 * @brief WebSocket을 통해 시스템 메트릭 데이터를 서버로 전송하는 클래스
 *
 * 이 클래스는 시스템에서 수집된 메트릭 데이터를 WebSocket 연결을 통해
 * 원격 서버로 주기적으로 전송하는 기능을 담당합니다.
 */
class DataSender
{
public:
    /**
     * @brief DataSender 클래스 생성자
     *
     * @param serverInfo 연결할 서버 정보 (주소, 포트 등)
     * @param dataQueue 전송할 시스템 메트릭 데이터가 저장된 스레드 안전 큐
     * @param user_id 사용자 식별자
     */
    DataSender(const ServerInfo &serverInfo, ThreadSafeQueue<SystemMetrics> &dataQueue, const string &user_id);

    /**
     * @brief DataSender 클래스 소멸자
     *
     * 모든 스레드를 종료하고 연결을 닫습니다.
     */
    ~DataSender();

    /**
     * @brief 서버에 WebSocket 연결을 시도합니다.
     *
     * @return 연결 성공 여부 (true: 성공, false: 실패)
     */
    bool connect();

    /**
     * @brief 서버와의 WebSocket 연결을 종료합니다.
     */
    void disconnect();

    /**
     * @brief 데이터 전송 프로세스를 시작합니다.
     *
     * @param intervalSeconds 데이터 전송 간격(초 단위), 기본값은 10초
     */
    void startSending(int intervalSeconds = 10);

    /**
     * @brief 데이터 전송 프로세스를 중지합니다.
     */
    void stopSending();

    /**
     * @brief 서버와의 연결 상태를 확인합니다.
     *
     * @return 연결 상태 (true: 연결됨, false: 연결 안됨)
     */
    bool isConnected() const { return isConnected_; }

private:
    /// @brief WebSocketPP 클라이언트 타입 정의
    typedef websocketpp::client<websocketpp::config::asio_client> WebsocketClient;

    /// @brief WebSocketPP 연결 핸들 타입 정의
    typedef websocketpp::connection_hdl WebsocketHandle;

    ServerInfo serverInfo_;                     ///< 서버 연결 정보
    ThreadSafeQueue<SystemMetrics> &dataQueue_; ///< 전송할 메트릭 데이터 큐
    ThreadSafeQueue<CommandResult> commandResultQueue_; ///< 커맨드 처리 결과 큐
    ThreadSafeQueue<CommandResult> commandQueue_; ///< 처리할 커맨드 작업 큐
    string user_id_;                            ///< 사용자 식별자

    WebsocketClient client_;           ///< WebSocket 클라이언트 인스턴스
    WebsocketHandle connectionHandle_; ///< 현재 활성화된 WebSocket 연결 핸들
    atomic<bool> isConnected_;         ///< 서버 연결 상태 플래그
    atomic<bool> running_;             ///< 데이터 전송 실행 상태 플래그

    thread clientThread_;  ///< WebSocket 통신 스레드
    thread senderThread_;  ///< 데이터 송신 스레드
    thread commandThread_; ///< 커맨드 처리 스레드

    // 워커 스레드 풀 관련 멤버
    vector<thread> workerThreads_; ///< 커맨드 처리 워커 스레드 풀
    const int numWorkers_ = 4;     ///< 워커 스레드 개수

    /**
     * @brief 지정된 간격으로 메트릭 데이터를 지속적으로 전송하는 루프
     *
     * @param intervalSeconds 전송 간격(초 단위)
     */
    void sendLoop(int intervalSeconds);

    /**
     * @brief 단일 시스템 메트릭 데이터를 서버로 전송합니다.
     *
     * @param metrics 전송할 시스템 메트릭 데이터
     * @return 전송 성공 여부 (true: 성공, false: 실패)
     */
    bool sendMetrics(const SystemMetrics &metrics);

    /**
     * @brief 서버로부터 수신된 메시지 처리 콜백 함수
     *
     * @param hdl WebSocket 연결 핸들
     * @param msg 수신된 메시지 포인터
     */
    void handleMessage(WebsocketHandle hdl, WebsocketClient::message_ptr msg);

    /**
     * @brief 커맨드 처리 쓰레드 함수
     */
    void commandProcessor();

    /**
     * @brief 커맨드를 처리하는 함수
     * 
     * @param command 처리할 커맨드
     * @return 처리 결과
     */
    CommandResult processCommand(const CommandResult& command);

    /**
     * @brief 워커 스레드 함수
     * 
     * 커맨드 큐에서 작업을 가져와 처리하는 함수
     */
    void workerThread();
};
