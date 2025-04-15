#pragma once

#include "models/command_result.h"
#include <string>

using namespace std;

namespace operations
{

    /**
     * @brief CLI 작업 클래스
     *
     * CLI 작업 모음
     */
    class CliOperations
    {
    public:
        /**
         * @brief CLI 명령어 실행 함수
         *
         * @param command 실행할 명령어
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult executeCli(const string &command, CommandResult &result);
    };

} // namespace operations