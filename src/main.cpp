/**
 * @file main.cpp
 * @brief 시스템 모니터링 애플리케이션의 메인 파일
 * @details 시스템 리소스(CPU, 메모리, 디스크, 네트워크, 프로세스) 정보를 수집하고
 *          ncurses UI를 통해 표시하거나 서버로 전송하는 기능을 제공합니다.
 * @author 시스템 모니터링 팀
 */

#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include "ui/dashboard_ncurses.h"
#include "collectors/cpu_collector.h"
#include "collectors/memory_collector.h"
#include "collectors/process_collector.h"
#include "collectors/disk_collector.h"
#include "collectors/network_collector.h"
#include "ui/i_dashboard.h"
#include "network/common/network_types.h"
#include "network/client/system_client.h"
#include "config/config.h"
#include "log/logger.h"
#include <atomic>

using namespace std;

/** @brief 프로그램 실행 상태를 제어하는 플래그 */
atomic<bool> running{true}; // volatile 대신 atomic 사용
/** @brief 서버 모드 사용 여부 플래그 */
bool useServer = false;

/** @brief 시스템 모니터링 UI 인터페이스 */
unique_ptr<IDashboard>
    dashboard;
/** @brief 시스템 정보 전송 클라이언트 */
unique_ptr<SystemClient> systemClient;

/**
 * @brief 시그널 처리 핸들러 함수
 * @param signum 수신된 시그널 번호
 * @details SIGINT, SIGTERM 시그널을 처리하여 프로그램을 안전하게 종료합니다.
 */
void signal_handler(int signum)
{
    running.store(false);
    LOG_INFO("종료 신호 수신 (시그널: {})", signum);
}

/**
 * @brief 프로그램의 메인 진입점
 * @param argc 명령행 인수 개수
 * @param argv 명령행 인수 배열
 * @return 프로그램 종료 코드
 * @details 다음 기능을 수행합니다:
 *          - 명령행 인수 파싱 및 설정
 *          - 로깅 초기화
 *          - 시그널 핸들러 설정
 *          - 서버 연결 또는 로컬 대시보드 표시
 */
int main(int argc, char **argv)
{
    /** @brief 로깅 시스템 초기화 */
    Logger::init();
    LOG_INFO("클라이언트 시작");

    /** @brief 명령행 인수 로깅 */
    string args;
    for (int i = 0; i < argc; i++)
    {
        args += argv[i];
        if (i < argc - 1)
            args += " ";
    }

    /** @brief 시그널 핸들러 설정 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /** @brief UI 업데이트 간격 (초) */
    const int updateInterval = 1;
    /** @brief 데이터 수집 간격 (초) */
    int collectionInterval = 5; // 기본 수집 간격 5초
    /** @brief 서버 데이터 전송 간격 (초) */
    int sendingInterval = 5;    // 기본 전송 간격 5초

    /** @brief 서버 연결 정보 */
    ServerInfo serverInfo;

    /** @brief 설정 관리 객체 */
    Config config;
    /** @brief 시스템 식별 키 */
    string systemKey;
    try
    {
        systemKey = config.getSystemKey();
    }
    catch (const exception &e)
    {
        LOG_ERROR("시스템 키를 가져오는 중 오류 발생: {}", e.what());
        systemKey = "default_key"; // 기본 키 설정
    }

    if (systemKey.empty())
    {
        LOG_WARN("시스템 키가 비어 있습니다. 기본 키를 사용합니다.");
        systemKey = "default_key";
    }

    /** @brief 사용자 식별자 */
    string user_id;
    
    /** @brief 명령줄 인수 처리 */
    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];
        if (arg == "-k")
        {
            if (i + 1 < argc)
            {
                user_id = argv[++i];
            }
        }
        else if (arg == "-h" || arg == "--help")
        {
            cout << "사용법: " << argv[0] << " [옵션]\n"
                 << "옵션:\n"
                 << "  -n, --ncurses        ncurses 모드 사용\n"
                 << "  -s, --server         서버 URL (예: @http://127.0.0.1:80)\n"
                 << "  -c, --collection     수집 간격 (초, 기본값: 5)\n"
                 << "  -t, --transmission   전송 간격 (초, 기본값: 5)\n"
                 << "  -h, --help          이 도움말 표시\n";
            return 0;
        }
        else if (arg == "-c" || arg == "--collection")
        {
            if (i + 1 < argc)
            {
                try
                {
                    collectionInterval = stoi(argv[++i]);
                }
                catch (const exception &e)
                {
                    cerr << "잘못된 수집 간격 값입니다. 기본값(5초)을 사용합니다.\n";
                    collectionInterval = 5;
                }
            }
        }
        else if (arg == "-t" || arg == "--transmission")
        {
            if (i + 1 < argc)
            {
                try
                {
                    sendingInterval = stoi(argv[++i]);
                }
                catch (const exception &e)
                {
                    cerr << "잘못된 전송 간격 값입니다. 기본값(5초)을 사용합니다.\n";
                    sendingInterval = 5;
                }
            }
        }
        else if (arg == "-s" || arg == "--server")
        {
            if (i + 1 < argc)
            {
                string serverUrl = argv[++i];

                if (serverUrl.substr(0, 7) == "http://" || serverUrl.substr(0, 8) == "https://")
                {
                    cerr << "잘못된 URL 형식입니다. 주소:포트 형식을 사용하세요.\n";
                    return 1;
                }

                size_t colonPos = serverUrl.find(':');
                if (colonPos != string::npos)
                {
                    serverInfo.address = serverUrl.substr(0, colonPos);
                    serverInfo.port = stoi(serverUrl.substr(colonPos + 1));
                    printf("서버 주소: %s, 포트: %d\n",
                           serverInfo.address.c_str(),
                           serverInfo.port);
                    useServer = true;
                }
                else
                {
                    cerr << "잘못된 URL 형식입니다. 주소:포트 형식을 사용하세요.\n";
                    return 1;
                }
            }
        }
    }

    if (collectionInterval > sendingInterval)
    {
        cerr << "수집 간격이 전송 간격보다 클 수 없습니다.\n";
        return 1;
    }

    /** @brief 서버 모드 실행 */
    if (useServer)
    {
        try
        {
            /** @brief 서버 클라이언트 초기화 및 연결 */
            systemClient = make_unique<SystemClient>(serverInfo, systemKey, collectionInterval, sendingInterval, user_id);
            systemClient->connect();

            /** @brief 프로그램 실행 유지 루프 */
            while (running.load())
            {
                this_thread::sleep_for(chrono::milliseconds(100));
            }

            LOG_INFO("종료 프로세스 시작");
            if (systemClient)
            {
                /** @brief 클라이언트 안전 종료 프로세스 */
                auto future = async(launch::async, [&]()
                                    { systemClient->disconnect(); });

                if (future.wait_for(chrono::seconds(5)) == future_status::timeout)
                {
                    LOG_ERROR("클라이언트 종료 시간 초과");
                }
            }
        }
        catch (const exception &e)
        {
            LOG_ERROR("서버 모드 실행 중 오류 발생: {}", e.what());
        }
    }
    /** @brief 로컬 UI 모드 실행 */
    else
    {
        try
        {
            /** @brief ncurses 기반 대시보드 초기화 */
            dashboard = make_unique<DashboardNcurses>();
            dashboard->init();

            /** @brief UI 갱신 루프 */
            while (running.load())
            {
                dashboard->handleInput();

                static auto lastUpdate = chrono::steady_clock::now();
                auto now = chrono::steady_clock::now();

                /** @brief 업데이트 간격 확인 및 UI 갱신 */
                if (chrono::duration_cast<chrono::seconds>(now - lastUpdate).count() >= updateInterval)
                {
                    dashboard->update();
                    lastUpdate = chrono::steady_clock::now();

                    /** @brief 업데이트 간격 조절을 위한 대기시간 계산 */
                    auto sleepTime = chrono::seconds(updateInterval) -
                                     (chrono::steady_clock::now() - lastUpdate);
                    if (sleepTime > chrono::milliseconds(0))
                    {
                        this_thread::sleep_for(sleepTime);
                    }
                }
                else
                {
                    /** @brief 업데이트가 필요없을 때 대기 */
                    this_thread::sleep_for(chrono::milliseconds(100));
                }
            }
        }
        catch (const exception &e)
        {
            // 예외 처리 (비어있음)
        }
    }

    /** @brief 종료 정리 작업 수행 */
    if (dashboard)
    {
        dashboard->cleanup();
    }

    LOG_INFO("클라이언트 종료");
    Logger::shutdown();

    return 0;
}