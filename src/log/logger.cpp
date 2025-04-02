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

        LOG_INFO("로거가 초기화되었습니다");
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        cerr << "로그 초기화 실패: " << ex.what() << endl;
        throw;
    }
}