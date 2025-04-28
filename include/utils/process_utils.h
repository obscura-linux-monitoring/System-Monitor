#ifndef PROCESS_UTILS_H
#define PROCESS_UTILS_H

#include <string>

using namespace std;

class ProcessUtils
{
public:
    /**
     * @brief 프로세스가 실행중인지 확인
     * @param pid 프로세스 ID
     * @return 프로세스가 실행중이면 true, 아니면 false
     */
    static bool isProcessRunning(int pid);

    /**
     * @brief 프로세스가 종료될 때까지 대기
     * @param pid 프로세스 ID
     * @param maxWaitSeconds 최대 대기 시간
     */
    static void waitForProcessToEnd(int pid, int maxWaitSeconds = 10);

    /**
     * @brief 쉘 명령어 이스케이프
     * @param cmd 쉘 명령어
     * @return 이스케이프된 쉘 명령어
     */
    static string escapeShellCommand(const string& cmd);
};


#endif