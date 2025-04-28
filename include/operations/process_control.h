#pragma once

#include "models/command_result.h"
#include <string>

using namespace std;

namespace operations
{

    /**
     * @brief 프로세스 제어 작업 클래스
     *
     * 프로세스 정지, 시작 등 프로세스 관련 작업 모음
     */
    class ProcessControl
    {
    public:
        /**
         * @brief 프로세스 강제 종료 함수
         *
         * @param processName 종료할 프로세스 PID
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult killProcess(int pid, CommandResult &result);

        /**
         * @brief 프로세스 재시작 함수
         *
         * @param pid 재시작할 프로세스 PID
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult restartProcess(int pid, CommandResult &result);
    };

} // namespace operations