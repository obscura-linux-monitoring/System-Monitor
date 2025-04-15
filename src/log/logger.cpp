/**
 * @file logger.cpp
 * @brief Logger 클래스 구현 파일
 * @details 시스템 로깅을 위한 Logger 클래스의 구현을 담고 있습니다.
 * @author 시스템 개발팀
 */

#include "log/logger.h"
#include <filesystem>
#include <iostream>
#include <time.h>
#include <string>

using namespace std;
namespace fs = filesystem;

/**
 * @brief Logger 클래스의 정적 로거 인스턴스
 * @details spdlog 라이브러리를 사용한 로거 인스턴스로, 시스템 전반에서 로깅에 사용됩니다.
 */
shared_ptr<spdlog::logger> Logger::s_logger;

/**
 * @brief 로그 전송 객체의 정적 인스턴스
 * @details WebSocket을 통해 로그를 서버로 전송하는 객체입니다.
 */
unique_ptr<LogSender> Logger::s_logSender;

/**
 * @brief 로그 큐의 정적 인스턴스
 * @details 로그 데이터를 저장하는 스레드 안전 큐입니다.
 */
ThreadSafeQueue<LogType> Logger::s_logQueue(1000); // 로그 큐 크기 1000

/**
 * @brief 원격 로깅 활성화 상태 플래그
 * @details 원격 로깅이 활성화되었는지 여부를 나타내는 플래그입니다.
 */
bool Logger::s_isRemoteLoggingEnabled = false;

/**
 * @brief 로거 초기화 함수
 * @details 로깅 시스템을 초기화합니다. 다음 작업을 수행합니다:
 *          - 현재 날짜 기반의 로그 파일명 생성
 *          - 로그 디렉토리 확인 및 생성
 *          - 파일 기반 싱크 설정
 *          - 로거 레벨 설정
 * @throws spdlog::spdlog_ex 로거 초기화 실패 시 예외 발생
 */
void Logger::init()
{
    try
    {
        // 현재 시간으로 파일 이름 생성
        time_t now = time(0);
        tm *ltm = localtime(&now);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y%m%d", ltm);

        // 원본 경로에서 log 디렉토리 경로 생성
        fs::path directory = fs::current_path() / "log";

        // 새로운 파일 이름 생성 (log 디렉토리 경로 + 시간 기반 파일 이름)
        string timeBasedLogFile = (directory / (string(buffer) + ".log")).string();

        // log 디렉토리 생성 (없는 경우에만)
        fs::create_directories(directory);

        // 콘솔 싱크와 파일 싱크 생성
        // auto consoleSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(
            timeBasedLogFile, 1024 * 1024 * 50, 1); // 50MB 크기 제한, 1개 파일만 유지

        // 로거 생성 및 싱크 설정
        /* s_logger = make_shared<spdlog::logger>("system_monitor",
                                                    spdlog::sinks_init_list{consoleSink, fileSink}); */
        // 로거 생성 및 싱크 설정 (콘솔 싱크 제외)
        s_logger = make_shared<spdlog::logger>(string(buffer),
                                               spdlog::sinks_init_list{fileSink});

        // 로그 레벨 설정
        s_logger->set_level(spdlog::level::debug);
        s_logger->flush_on(spdlog::level::debug);

        // 대안 1: Visual Studio 스타일
        s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %@: [%!] %v");

        LOG_INFO("로거가 초기화되었습니다");
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        cerr << "로그 초기화 실패: " << ex.what() << endl;
        throw;
    }
}

/**
 * @brief 원격 로깅 초기화 함수
 * @param serverInfo 원격 로깅 서버 정보
 * @param nodeId 노드 식별자
 * @param intervalSeconds 로그 전송 간격 (초)
 * @details 기본 로깅 시스템을 초기화하고 원격 로깅을 설정합니다.
 * @throws spdlog::spdlog_ex 로거 초기화 실패 시 예외 발생
 */
void Logger::initWithRemoteLogging(const ServerInfo &serverInfo, const string &nodeId, int intervalSeconds)
{
    try
    {
        // 캡처 싱크 추가
        auto captureSink = make_shared<LogCaptureSinkMt>(s_logQueue, nodeId);
        s_logger->sinks().push_back(captureSink);

        // 로그 전송기 생성 및 시작
        s_logSender = make_unique<LogSender>(serverInfo, s_logQueue);
        if (s_logSender->connect())
        {
            s_logSender->startSending(intervalSeconds); // 5초 간격으로 전송
            s_isRemoteLoggingEnabled = true;
            LOG_INFO("원격 로깅이 활성화되었습니다");
        }
        else
        {
            LOG_ERROR("원격 로깅 활성화 실패");
        }
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        cerr << "원격 로깅 초기화 실패: " << ex.what() << endl;
        throw;
    }
}

/**
 * @brief 로거 종료 함수
 * @details 로거를 종료하고 모든 버퍼를 비우고 파일을 닫습니다.
 */
void Logger::shutdown()
{
    if (s_logSender && s_isRemoteLoggingEnabled)
    {
        s_logSender->stopSending();
        s_logSender->disconnect();
        s_logSender.reset();
    }

    if (s_logger)
    {
        s_logger->flush();
        spdlog::shutdown();
    }
}