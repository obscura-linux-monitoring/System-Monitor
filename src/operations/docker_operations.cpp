#include "operations/docker_operations.h"
#include "log/logger.h"
#include <cstdlib>
#include <string>

using namespace std;

namespace operations
{

    CommandResult DockerOperations::startContainer(const string &containerName, CommandResult &result)
    {
        LOG_INFO("Docker 컨테이너 시작 작업 실행: {}", containerName);

        string command = "sudo docker start " + containerName;
        int exitCode = system(command.c_str());

        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "Docker 컨테이너 시작 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "Docker 컨테이너 시작 명령 수행 완료";

        return result;
    }

    CommandResult DockerOperations::stopContainer(const string &containerName, CommandResult &result)
    {
        LOG_INFO("Docker 컨테이너 정지 작업 실행: {}", containerName);

        string command = "sudo docker stop " + containerName;
        int exitCode = system(command.c_str());

        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "Docker 컨테이너 정지 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "Docker 컨테이너 정지 명령 수행 완료";

        return result;
    }

    CommandResult DockerOperations::restartContainer(const string &containerName, CommandResult &result)
    {
        LOG_INFO("Docker 컨테이너 재시작 작업 실행: {}", containerName);

        string command = "sudo docker restart " + containerName;
        int exitCode = system(command.c_str());

        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "Docker 컨테이너 재시작 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "Docker 컨테이너 재시작 명령 수행 완료";

        return result;
    }

    CommandResult DockerOperations::deleteContainer(const string &containerName, CommandResult &result)
    {
        LOG_INFO("Docker 컨테이너 삭제 작업 실행: {}", containerName);

        string command = "sudo docker rm " + containerName;
        int exitCode = system(command.c_str());

        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "Docker 컨테이너 삭제 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "Docker 컨테이너 삭제 명령 수행 완료";

        return result;
    }

} // namespace operations