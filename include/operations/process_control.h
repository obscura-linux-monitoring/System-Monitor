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
         * @brief 프로세스 정지 함수
         *
         * @param processName 정지할 프로세스 이름 또는 PID
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult stopProcess(const string &processName);

        /**
         * @brief 프로세스 강제 종료 함수
         *
         * @param processName 종료할 프로세스 이름 또는 PID
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult killProcess(const string &processName);
    };

} // namespace operations