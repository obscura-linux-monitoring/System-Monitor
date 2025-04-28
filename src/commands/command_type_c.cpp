#include "commands/command_types.h"
#include "log/logger.h"
#include "operations/process_control.h"

using namespace std;

/**
 * @brief C 타입 명령어를 실행하는 함수
 *
 * 이 함수는 프로세스 작업과 같은 C 타입 명령어를 처리합니다.
 * 명령어 실행 과정에서 로그를 남기고 적절한 결과 상태를 설정합니다.
 *
 * @param command 실행할 명령어 정보가 담긴 CommandResult 객체
 * @return CommandResult 명령어 실행 결과를 담은 객체
 * @see CommandResult
 * @note 현재 구현에서는 항상 성공(1) 상태를 반환합니다
 */
CommandResult CommandTypeCExecutor::execute(const CommandResult &command)
{
    CommandResult result = command;

    LOG_INFO("C 타입 커맨드 실행 중: ID={}", command.commandID);

    int pid = 0;
    try
    {
        pid = stoi(command.target);
    }
    catch (const exception &e)
    {
        LOG_ERROR("유효하지 않은 프로세스 ID: {}", e.what());
        result.resultStatus = 0;
        result.resultMessage = "유효하지 않은 프로세스 ID";
    }

    if (command.commandStatus == 1) // 중지
    {
        LOG_INFO("정지 명령어 실행 중: ID={}", command.commandID);
        result = operations::ProcessControl::killProcess(pid, result);
    }
    else if (command.commandStatus == 2) // 재시작
    {
        LOG_INFO("재시작 명령어 실행 중: ID={}", command.commandID);
        result = operations::ProcessControl::restartProcess(pid, result);
    }
    else
    {
        LOG_WARN("유효하지 않은 명령어 타입: ID={}, TYPE={}", command.commandID, command.commandType);
        result.resultStatus = 0;
        result.resultMessage = "유효하지 않은 명령어 타입";
    }

    return result;
}