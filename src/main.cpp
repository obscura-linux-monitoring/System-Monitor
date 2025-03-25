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

atomic<bool> running{true}; // volatile 대신 atomic 사용
bool useServer = false;

unique_ptr<IDashboard>
    dashboard;
unique_ptr<SystemClient> systemClient;

void signal_handler(int signum)
{
    running.store(false);
    LOG_INFO("종료 신호 수신 (시그널: {})", signum);
}

int main(int argc, char **argv)
{
    Logger::init();

    // argv 값들을 문자열로 변환하여 로깅
    string args;
    for (int i = 0; i < argc; i++)
    {
        args += argv[i];
        if (i < argc - 1)
            args += " ";
    }

    // 시그널 핸들러 설정
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    const int updateInterval = 1;
    int collectionInterval = 5; // 기본 수집 간격 5초
    int sendingInterval = 5;    // 기본 전송 간격 5초

    ServerInfo serverInfo;

    Config config;
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

    // 명령줄 인수 처리
    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];
        if (arg == "-n" || arg == "--ncurses")
        {
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

    if (useServer)
    {
        try
        {
            systemClient = make_unique<SystemClient>(serverInfo, systemKey, collectionInterval, sendingInterval);
            systemClient->connect();

            while (running.load())
            {
                this_thread::sleep_for(chrono::milliseconds(100));
            }

            LOG_INFO("종료 프로세스 시작");
            if (systemClient)
            {
                // 타임아웃을 설정하여 안전하게 종료
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
    else
    {
        // 메인 로직 구현
        try
        {
            dashboard = make_unique<DashboardNcurses>();

            dashboard->init();

            while (running.load())
            {
                dashboard->handleInput();

                static auto lastUpdate = chrono::steady_clock::now();
                auto now = chrono::steady_clock::now();

                if (chrono::duration_cast<chrono::seconds>(now - lastUpdate).count() >= updateInterval)
                {
                    dashboard->update();
                    lastUpdate = chrono::steady_clock::now();

                    // 다음 업데이트까지 남은 시간 계산
                    auto sleepTime = chrono::seconds(updateInterval) -
                                     (chrono::steady_clock::now() - lastUpdate);
                    if (sleepTime > chrono::milliseconds(0))
                    {
                        this_thread::sleep_for(sleepTime);
                    }
                }
                else
                {
                    // 업데이트가 필요없을 때는 더 오래 대기
                    this_thread::sleep_for(chrono::milliseconds(100));
                }
            }
        }
        catch (const exception &e)
        {
        }
    }

    if (dashboard)
    {
        dashboard->cleanup();
    }

    return 0;
}