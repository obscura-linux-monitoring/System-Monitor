#pragma once

#include "models/command_result.h"
#include <string>

using namespace std;

namespace operations
{

    /**
     * @brief Docker 작업 클래스
     *
     * Docker 컨테이너, 이미지 등 관련 작업 모음
     */
    class DockerOperations
    {
    public:
        /**
         * @brief Docker 컨테이너 시작 함수
         *
         * @param containerName 시작할 컨테이너 이름
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult startContainer(const string &containerName);

        /**
         * @brief Docker 컨테이너 정지 함수
         *
         * @param containerName 정지할 컨테이너 이름
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult stopContainer(const string &containerName);

        /**
         * @brief Docker 컨테이너 재시작 함수
         *
         * @param containerName 재시작할 컨테이너 이름
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult restartContainer(const string &containerName);

        /**
         * @brief Docker 컨테이너 삭제 함수
         *
         * @param containerName 삭제할 컨테이너 이름
         * @return CommandResult 작업 실행 결과
         */
        static CommandResult deleteContainer(const string &containerName);
    };

} // namespace operations