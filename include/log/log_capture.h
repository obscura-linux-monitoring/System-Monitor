#pragma once

#include "log/log_type.h"
#include "common/thread_safe_queue.h"
#include <spdlog/sinks/base_sink.h>
#include <mutex>
#include <string>

using namespace std;
using namespace spdlog;

template <typename Mutex>
class LogCaptureSink : public spdlog::sinks::base_sink<Mutex>
{
public:
    LogCaptureSink(ThreadSafeQueue<LogType> &queue, const string &nodeId)
        : queue_(queue), nodeId_(nodeId) {}

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override
    {
        // 로그 메시지와 시간을 캡처
        LogType logData;
        logData.node_id = nodeId_;

        // 타임스탬프 포맷
        time_t raw_time = chrono::system_clock::to_time_t(msg.time);
        char timeStr[30];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&raw_time));
        logData.timestamp = string(timeStr);

        // 로그 메시지 섹션(레벨)
        switch (msg.level)
        {
        case level::trace:
            logData.level = "TRACE";
            break;
        case level::debug:
            logData.level = "DEBUG";
            break;
        case level::info:
            logData.level = "INFO";
            break;
        case level::warn:
            logData.level = "WARN";
            break;
        case level::err:
            logData.level = "ERROR";
            break;
        case level::critical:
            logData.level = "CRITICAL";
            break;
        default:
            logData.level = "UNKNOWN";
            break;
        }

        // 로그 메시지 내용
        string fullContent = string(msg.payload.data(), msg.payload.size());
        logData.content = fullContent;

        // 큐에 추가 (예외 처리 추가)
        try
        {
            bool success = queue_.push(logData);
            if (!success)
            {
                // fprintf로 직접 출력 (로그 루프 방지용)
                fprintf(stderr, "로그 큐 추가 실패: %s\n", logData.content.c_str());
            }
        }
        catch (const exception &e)
        {
            fprintf(stderr, "로그 캡처 예외: %s\n", e.what());
        }
    }

    void flush_() override {}

private:
    ThreadSafeQueue<LogType> &queue_;
    string nodeId_;
};

using LogCaptureSinkMt = LogCaptureSink<mutex>;