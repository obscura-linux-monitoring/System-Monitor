#pragma once

#include "network/common/network_types.h"
#include "log/log_type.h"
#include "common/thread_safe_queue.h"
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <thread>
#include <atomic>
#include <string>

using namespace std;

class LogSender
{
public:
    LogSender(const ServerInfo &serverInfo, ThreadSafeQueue<LogType> &logQueue);
    ~LogSender();

    bool connect();
    void disconnect();
    void startSending(int intervalSeconds = 5);
    void stopSending();
    bool isConnected() const { return isConnected_; }

private:
    typedef websocketpp::client<websocketpp::config::asio_client> WebsocketClient;
    typedef websocketpp::connection_hdl WebsocketHandle;

    ServerInfo serverInfo_;
    ThreadSafeQueue<LogType> &logQueue_;
    WebsocketClient client_;
    WebsocketHandle connectionHandle_;
    atomic<bool> isConnected_;
    thread clientThread_;
    thread senderThread_;

    void sendLoop(int intervalSeconds);
    bool sendLogs(const vector<LogType> &logs);
    string formatRFC3339(const string &timestamp);
};