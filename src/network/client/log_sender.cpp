#include "network/client/log_sender.h"
#include "log/logger.h"
#include <iostream>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

extern atomic<bool> running;

LogSender::LogSender(const ServerInfo &serverInfo, ThreadSafeQueue<LogType> &logQueue)
    : serverInfo_(serverInfo), logQueue_(logQueue),
      isConnected_(false)
{
    client_.init_asio();
    client_.clear_access_channels(websocketpp::log::alevel::all);
    client_.clear_error_channels(websocketpp::log::elevel::all);
}

LogSender::~LogSender()
{
    stopSending();
    disconnect();
}

bool LogSender::connect()
{
    string uri = "ws://" + serverInfo_.address + ":" +
                 to_string(serverInfo_.port) + "/ws/logs";

    try
    {
        websocketpp::lib::error_code ec;
        WebsocketClient::connection_ptr con = client_.get_connection(uri, ec);

        if (ec)
        {
            LOG_ERROR("로그 WebSocket 연결 오류: {}", ec.message());
            return false;
        }

        connectionHandle_ = con->get_handle();

        client_.set_fail_handler([this](WebsocketHandle hdl)
                                 { 
                                    (void)hdl;
                                    isConnected_ = false; });

        client_.set_close_handler([this](WebsocketHandle hdl)
                                  {
                                      (void)hdl;
                                      isConnected_ = false; });

        client_.connect(con);
        clientThread_ = thread([this]()
                               { this->client_.run(); });
        isConnected_.store(true);

        LOG_INFO("로그 WebSocket 서버({})에 연결되었습니다.", uri);
        return true;
    }
    catch (const exception &e)
    {
        LOG_ERROR("로그 WebSocket 연결 예외: {}", e.what());
        return false;
    }
}

void LogSender::disconnect()
{
    if (isConnected_.load())
    {
        try
        {
            websocketpp::lib::error_code ec;
            client_.close(connectionHandle_, websocketpp::close::status::normal, "정상 종료", ec);
            this_thread::sleep_for(chrono::milliseconds(100));
            client_.stop();

            if (clientThread_.joinable())
            {
                clientThread_.join();
            }

            isConnected_.store(false);
            LOG_INFO("로그 WebSocket 연결이 종료되었습니다.");
        }
        catch (const exception &e)
        {
            LOG_ERROR("로그 WebSocket 연결 종료 중 오류: {}", e.what());
        }
    }
}

void LogSender::startSending(int intervalSeconds)
{
    senderThread_ = thread(&LogSender::sendLoop, this, intervalSeconds);
    LOG_INFO("로그 전송 쓰레드가 시작되었습니다 (간격: {}초)", intervalSeconds);
}

void LogSender::stopSending()
{
    if (senderThread_.joinable())
    {
        senderThread_.join();
    }

    LOG_INFO("로그 전송 쓰레드가 종료되었습니다.");
}

void LogSender::sendLoop(int intervalSeconds)
{
    LOG_INFO("로그 전송 루프 시작");
    while (running.load() && isConnected_.load())
    {
        auto startTime = chrono::steady_clock::now();

        // 큐에서 로그 데이터를 모아서 한번에 전송
        vector<LogType> logs;
        LogType log;

        // 큐 상태 로깅 추가
        LOG_DEBUG("로그 큐 상태: 크기={}", logQueue_.size());

        // 최대 100개까지 로그를 모음
        int fetched = 0;
        for (int i = 0; i < 100 && logQueue_.try_pop(log, chrono::milliseconds(10)); i++)
        {
            logs.push_back(log);
            fetched++;
        }

        // 로그가 있으면 전송
        if (!logs.empty())
        {
            LOG_INFO("로그 {} 개 가져옴, 전송 시도", fetched);
            sendLogs(logs);
        }
        else
        {
            LOG_DEBUG("전송할 로그 없음");
        }

        // 전송 간격 조절
        auto endTime = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        auto sleepTime = chrono::seconds(intervalSeconds) - elapsed;

        if (sleepTime > chrono::milliseconds(0))
        {
            this_thread::sleep_for(sleepTime);
        }
    }
    LOG_INFO("로그 전송 루프 종료");
}

string LogSender::formatRFC3339(const string &timestamp)
{
    // "2025-04-15 16:14:49" 형식을 "2025-04-15T16:14:49Z" 형식으로 변환
    string result = timestamp;
    // 공백을 T로 변환
    for (size_t i = 0; i < result.length(); ++i)
    {
        if (result[i] == ' ')
        {
            result[i] = 'T';
        }
    }
    // Z 추가
    result += 'Z';
    return result;
}

bool LogSender::sendLogs(const vector<LogType> &logs)
{
    if (!isConnected_.load() || logs.empty())
        return false;

    try
    {
        // JSON 배열로 변환
        json logsArray = json::array();

        for (const auto &log : logs)
        {
            json logJson;
            logJson["node_id"] = log.node_id;
            logJson["timestamp"] = formatRFC3339(log.timestamp); // "2025-04-15T16:14:49Z" 형식으로 변환
            logJson["level"] = log.level;
            logJson["content"] = log.content;
            logsArray.push_back(logJson);
        }

        json payload;
        payload["logs"] = logsArray;

        string data = payload.dump();

        // WebSocket으로 전송
        websocketpp::lib::error_code ec;
        client_.send(connectionHandle_, data, websocketpp::frame::opcode::text, ec);

        if (ec)
        {
            LOG_ERROR("로그 전송 오류: {}", ec.message());
            return false;
        }

        LOG_DEBUG("로그 {} 개 전송 완료", logs.size());
        return true;
    }
    catch (const exception &e)
    {
        LOG_ERROR("로그 전송 중 예외 발생: {}", e.what());
        return false;
    }
}