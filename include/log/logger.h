/**
 * @file logger.h
 * @brief 애플리케이션의 로깅 기능을 제공하는 로거 클래스 정의
 * @details spdlog 라이브러리를 사용하여 로깅 기능을 구현합니다.
 */

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>

using namespace std;

/**
 * @class Logger
 * @brief 로깅 기능을 제공하는 싱글톤 클래스
 * @details 애플리케이션 전반에서 로그를 기록하기 위한 인터페이스를 제공합니다.
 */
class Logger
{
public:
    /**
     * @brief 로거를 초기화하는 정적 메서드
     * @details 로깅 파일을 생성하고 로거의 설정을 구성합니다.
     */
    static void init();

    /**
     * @brief 로거 종료 함수
     * @details 로거를 종료하고 모든 버퍼를 비우고 파일을 닫습니다.
     */
    static void shutdown() {
        if (s_logger) {
            s_logger->flush();
            spdlog::shutdown();
        }
    }

    /**
     * @brief 로거 인스턴스를 반환하는 정적 메서드
     * @return 공유 포인터로 래핑된 spdlog 로거 인스턴스
     */
    static shared_ptr<spdlog::logger> &getLogger() { return s_logger; }

private:
    /**
     * @brief spdlog 로거의 정적 인스턴스
     */
    static shared_ptr<spdlog::logger> s_logger;
};

/**
 * @brief TRACE 수준의 로그 메시지를 기록하는 매크로
 * @param ... 로그 메시지와 포맷 인자
 */
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(Logger::getLogger(), __VA_ARGS__)

/**
 * @brief DEBUG 수준의 로그 메시지를 기록하는 매크로
 * @param ... 로그 메시지와 포맷 인자
 */
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(Logger::getLogger(), __VA_ARGS__)

/**
 * @brief INFO 수준의 로그 메시지를 기록하는 매크로
 * @param ... 로그 메시지와 포맷 인자
 */
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(Logger::getLogger(), __VA_ARGS__)

/**
 * @brief WARN 수준의 로그 메시지를 기록하는 매크로
 * @param ... 로그 메시지와 포맷 인자
 */
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(Logger::getLogger(), __VA_ARGS__)

/**
 * @brief ERROR 수준의 로그 메시지를 기록하는 매크로
 * @param ... 로그 메시지와 포맷 인자
 */
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(Logger::getLogger(), __VA_ARGS__)

/**
 * @brief CRITICAL 수준의 로그 메시지를 기록하는 매크로
 * @param ... 로그 메시지와 포맷 인자
 */
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(Logger::getLogger(), __VA_ARGS__)