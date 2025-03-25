#include "network/client/data_sender.h"
#include "log/logger.h"
#include <iostream>

using namespace std;

DataSender::DataSender(const ServerInfo &serverInfo, ThreadSafeQueue<SystemMetrics> &dataQueue)
    : serverInfo_(serverInfo), dataQueue_(dataQueue), isConnected_(false), running_(false)
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

    cout << logMessage << endl;

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
    // 서버로부터의 메시지 처리
    string data = msg->get_payload();

    try
    {
        // 수신된 데이터 로깅 (디버깅 용도)
        time_t now = time(nullptr);
        char timestamp[30];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

        cout << "[" << timestamp << "] 메시지 수신: " << (data.length() > 100 ? data.substr(0, 100) + "..." : data) << endl;

        // JSON 파싱 및 명령 처리 (예시)
        // 서버에서 명령을 전송하는 경우 처리할 수 있음
        // 예: {"command": "set_interval", "value": 30}

        // 실제 구현은 서버와의 프로토콜에 따라 달라질 수 있음
        if (data.find("command") != string::npos)
        {
            // 명령 처리 로직
            if (data.find("set_interval") != string::npos)
            {
                // 수집 간격 변경 명령 처리
                // 구현 필요
            }
            else if (data.find("pause") != string::npos)
            {
                // 일시 중지 명령 처리
                // 구현 필요
            }
            else if (data.find("resume") != string::npos)
            {
                // 재개 명령 처리
                // 구현 필요
            }
        }
    }
    catch (const exception &e)
    {
        cerr << "메시지 처리 중 오류 발생: " << e.what() << endl;
    }
}
