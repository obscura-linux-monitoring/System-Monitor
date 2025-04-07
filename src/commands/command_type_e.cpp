#include "commands/command_types.h"
#include "log/logger.h"
#include "operations/docker_operations.h"
using namespace std;

/**
 * @brief E 타입 명령어를 실행하는 함수
 *
 * 이 함수는 docker 컨테이너 작업과 같은 E 타입 명령어를 처리합니다.
 * 명령어 실행 과정에서 로그를 남기고 적절한 결과 상태를 설정합니다.
 *
 * @param command 실행할 명령어 정보가 담긴 CommandResult 객체
 * @return CommandResult 명령어 실행 결과를 담은 객체
 * @see CommandResult
 * @note 현재 구현에서는 항상 성공(1) 상태를 반환합니다
 */
CommandResult CommandTypeEExecutor::execute(const CommandResult &command)
{
    CommandResult result = command;

    LOG_INFO("E 타입 커맨드 실행 중: ID={}", command.commandID);

    if (command.commandStatus == 1) // 중지
    {
        LOG_INFO("중지 명령어 실행 중: ID={}", command.commandID);
        result = operations::DockerOperations::stopContainer(command.target);
    }
    else if (command.commandStatus == 2) // 시작
    {
        LOG_INFO("시작 명령어 실행 중: ID={}", command.commandID);
        result = operations::DockerOperations::startContainer(command.target);
    }
    else if (command.commandStatus == 3) // 재시작
    {
        LOG_INFO("재시작 명령어 실행 중: ID={}", command.commandID);
        result = operations::DockerOperations::restartContainer(command.target);
    }
    else if (command.commandStatus == 4) // 삭제
    {
        LOG_INFO("삭제 명령어 실행 중: ID={}", command.commandID);
        result = operations::DockerOperations::deleteContainer(command.target);
    }
    else
    {
        LOG_WARN("유효하지 않은 명령어 타입: ID={}, TYPE={}", command.commandID, command.commandType);
        result.resultStatus = 0;
        result.resultMessage = "유효하지 않은 명령어 타입";
    }

    result.commandID = command.commandID;
    result.nodeID = command.nodeID;
    result.commandType = command.commandType;
    result.commandStatus = command.commandStatus;
    result.target = command.target;

    return result;
}