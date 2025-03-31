#include "network/client/data_sender.h"
#include "log/logger.h"
#include <iostream>

using namespace std;

DataSender::DataSender(const ServerInfo &serverInfo, ThreadSafeQueue<SystemMetrics> &dataQueue, const string &user_id)
    : serverInfo_(serverInfo), dataQueue_(dataQueue), user_id_(user_id), isConnected_(false), running_(false)
{
    // WebSocket 클라이언트 초기화
    client_.init_asio();

    // 로깅 비활성화
    client_.clear_access_channels(websocketpp::log::alevel::all);
    client_.clear_error_channels(websocketpp::log::elevel::all);

    // 콜백 설정
    client_.set_message_handler(
        [this](WebsocketHandle hdl, WebsocketClient::message_ptr msg)
        {
            this->handleMessage(hdl, msg);
        });
}

DataSender::~DataSender()
{
    stopSending();
    disconnect();
}

bool DataSender::connect()
{
    // URI에 '/ws' 경로 추가
    string uri = "ws://" + serverInfo_.address + ":" +
                 to_string(serverInfo_.port) + "/ws";

    try
    {
        websocketpp::lib::error_code ec;
        WebsocketClient::connection_ptr con = client_.get_connection(uri, ec);

        if (ec)
        {
            return false;
        }

        // 연결 핸들 저장
        connectionHandle_ = con->get_handle();

        // 연결 실패 핸들러 설정
        client_.set_fail_handler([this](WebsocketHandle hdl)
                                 { isConnected_ = false; });

        // 연결 종료 핸들러 설정
        client_.set_close_handler([this](WebsocketHandle hdl)
                                  { isConnected_ = false; });

        // 연결 시도
        client_.connect(con);

        // 비동기로 실행 (스레드 저장)
        clientThread_ = thread([this]()
                               { this->client_.run(); });

        isConnected_ = true;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void DataSender::disconnect()
{
    if (isConnected_)
    {
        try
        {
            // 먼저 연결을 정상적으로 종료
            websocketpp::lib::error_code ec;
            client_.close(connectionHandle_, websocketpp::close::status::normal, "정상 종료", ec);

            // 잠시 대기하여 종료 메시지가 전송될 시간을 줌
            this_thread::sleep_for(chrono::milliseconds(100));

            // asio 서비스 중지
            client_.stop();

            if (clientThread_.joinable())
            {
                clientThread_.join();
            }

            isConnected_ = false;
            LOG_INFO("WebSocket 연결이 정상적으로 종료되었습니다.");
        }
        catch (const exception &e)
        {
            LOG_ERROR("WebSocket 연결 종료 중 오류 발생: {}", e.what());
        }
    }
}

void DataSender::startSending(int intervalSeconds)
{
    if (running_)
        return;

    running_ = true;
    senderThread_ = thread(&DataSender::sendLoop, this, intervalSeconds);
}

void DataSender::stopSending()
{
    if (!running_)
        return;

    running_ = false;

    if (senderThread_.joinable())
    {
        senderThread_.join();
    }
}

void DataSender::sendLoop(int intervalSeconds)
{
    while (running_ && isConnected_)
    {
        auto startTime = chrono::steady_clock::now();

        // 큐에서 데이터 가져와서 전송
        SystemMetrics metrics;
        if (dataQueue_.pop(metrics))
        {
            metrics.user_id = user_id_;
            sendMetrics(metrics);
        }

        // 송신 간격 조절
        auto endTime = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        auto sleepTime = chrono::seconds(intervalSeconds) - elapsed;

        if (sleepTime > chrono::milliseconds(0))
        {
            this_thread::sleep_for(sleepTime);
        }
    }
}

bool DataSender::sendMetrics(const SystemMetrics &metrics)
{
    if (!isConnected_)
        return false;

    // 전송 시작 시간 기록
    auto sendStartTime = chrono::steady_clock::now();
    time_t now = time(nullptr);
    char startTimestamp[30];
    strftime(startTimestamp, sizeof(startTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // 메트릭을 JSON으로 변환
    string data = SystemMetricsUtil::toJson(metrics);

    // WebSocket으로 전송
    websocketpp::lib::error_code ec;
    client_.send(connectionHandle_, data, websocketpp::frame::opcode::text, ec);

    // 전송 종료 시간 기록 및 소요 시간 계산
    auto sendEndTime = chrono::steady_clock::now();
    auto sendDuration = chrono::duration_cast<chrono::milliseconds>(sendEndTime - sendStartTime);

    // 전송 시간 정보 출력
    char endTimestamp[30];
    time_t endTime = time(nullptr);
    strftime(endTimestamp, sizeof(endTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&endTime));

    string logMessage = "[전송] 시작: " + string(startTimestamp) +
                        ", 종료: " + string(endTimestamp) +
                        ", 소요 시간: " + to_string(sendDuration.count()) + "ms, 데이터 크기: " +
                        to_string(data.length()) + "바이트" +
                        (ec ? ", 오류 발생: " + ec.message() : ", 성공");

    if (ec)
    {
        LOG_ERROR(logMessage);
    }
    else
    {
        LOG_INFO(logMessage);
    }
    LOG_INFO("데이터 : \n" + data);

    return !ec;
}

void DataSender::handleMessage(WebsocketHandle hdl, WebsocketClient::message_ptr msg)
{
    (void)hdl;
    // 서버로부터의 메시지 처리
    string data = msg->get_payload();

    try
    {
        LOG_INFO("메시지 수신: {}", data);
    }
    catch (const exception &e)
    {
        LOG_ERROR("메시지 처리 중 오류 발생: {}", e.what());
    }
}
