#pragma once

#include "models/command_result.h"

using namespace std;

namespace operations
{

    /**
     * @brief 자기 자신 관리 작업 클래스
     *
     * 프로그램 자체의 종료, 재시작 등을 처리하는 작업 모음
     */
    class SelfManagement
    {
    public:
        /**
         * @brief 프로그램 종료 함수
         *
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult shutdown(CommandResult &result);

        /**
         * @brief 프로그램 재시작 함수
         *
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult restart(CommandResult &result);
    };

} // namespace operations