#pragma once

#include "models/command_result.h"
#include <string>

using namespace std;

namespace operations
{

    /**
     * @brief 서비스 제어 작업 클래스
     *
     * 시스템 서비스 시작, 정지, 재시작 등 서비스 관련 작업 모음
     */
    class ServiceControl
    {
    public:
        /**
         * @brief 서비스 시작 함수
         *
         * @param serviceName 시작할 서비스 이름
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult startService(const string &serviceName, CommandResult &result);

        /**
         * @brief 서비스 정지 함수
         *
         * @param serviceName 정지할 서비스 이름
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult stopService(const string &serviceName, CommandResult &result);

        /**
         * @brief 서비스 재시작 함수
         *
         * @param serviceName 재시작할 서비스 이름
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult restartService(const string &serviceName, CommandResult &result);

        /**
         * @brief 서비스 제거 함수
         *
         * @param serviceName 제거할 서비스 이름
         * @param result 작업 실행 결과
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult removeService(const string &serviceName, CommandResult &result);

        /**
         * @brief 데몬 재시작 함수
         *
         * @return CommandResult 작업 실행 결과
         */
        static int daemonReload();
    };

} // namespace operations