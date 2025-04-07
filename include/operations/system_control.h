#pragma once

#include "models/command_result.h"

using namespace std;

namespace operations
{

    /**
     * @brief 시스템 제어 작업 클래스
     *
     * PC 종료, 재시작 등 시스템 제어 작업 모음
     */
    class SystemControl
    {
    public:
        /**
         * @brief PC 종료 함수
         *
         * @param delaySeconds 종료 전 대기 시간(초)
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult shutdownPC(int delaySeconds, CommandResult &result);

        /**
         * @brief PC 재시작 함수
         *
         * @param delaySeconds 재시작 전 대기 시간(초)
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult restartPC(int delaySeconds, CommandResult &result);
    };

} // namespace operations