#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>

class Logger
{
public:
    static void init();
    static std::shared_ptr<spdlog::logger> &getLogger() { return s_logger; }

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

// 로깅 매크로 정의
#define LOG_TRACE(...) Logger::getLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::getLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::getLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) Logger::getLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::getLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::getLogger()->critical(__VA_ARGS__)