/**
 * @file command_executor.cpp
 * @brief 명령 실행기 및 처리기 구현
 * @author
 */

#include "commands/command_executor.h"
#include "commands/command_types.h"
#include "log/logger.h"
#include <unordered_map>

using namespace std;

/// @brief 실행기 객체를 저장하는 정적 맵
static unordered_map<string, unique_ptr<ICommandExecutor>> executors_;
/// @brief 실행기 초기화 상태를 나타내는 플래그
static bool initialized_ = false;

/**
 * @brief 명령 실행기 팩토리 클래스의 초기화 상태를 확인한다
 * @return 초기화 되었으면 true, 아니면 false
 */
bool CommandExecutorFactory::isInitialized()
{
    return initialized_;
}

/**
 * @brief 명령 실행기 팩토리를 초기화한다
 *
 * 모든 지원되는 명령 타입에 대한 실행기를 생성하고 맵에 등록한다
 */
void CommandExecutorFactory::initialize()
{
    if (!initialized_)
    {
        executors_["a"] = make_unique<CommandTypeAExecutor>();
        executors_["b"] = make_unique<CommandTypeBExecutor>();
        executors_["c"] = make_unique<CommandTypeCExecutor>();
        executors_["d"] = make_unique<CommandTypeDExecutor>();
        executors_["e"] = make_unique<CommandTypeEExecutor>();
        initialized_ = true;
    }
}

/**
 * @brief 지정된 명령 타입에 해당하는 실행기를 반환한다
 *
 * @param commandType 명령 타입 문자열
 * @return ICommandExecutor* 명령 실행기 포인터 (실패시 nullptr)
 */
ICommandExecutor *CommandExecutorFactory::getExecutor(const string &commandType)
{
    if (!isInitialized())
    {
        initialize();
    }

    auto it = executors_.find(commandType);
    if (it != executors_.end())
    {
        return it->second.get();
    }

    LOG_WARN("알 수 없는 커맨드 타입: {}", commandType);
    return nullptr;
}

/**
 * @brief 주어진 명령을 처리하고 결과를 반환한다
 *
 * @param command 처리할 명령 객체
 * @return CommandResult 명령 처리 결과
 * @throws exception 명령 처리 중 예외 발생 시
 */
CommandResult CommandProcessor::process(const CommandResult &command)
{
    CommandResult result = command;

    try
    {
        // 커맨드 타입에 맞는 실행기 가져오기
        auto executor = CommandExecutorFactory::getExecutor(command.commandType);

        // 실행기가 있으면 실행
        if (executor)
        {
            LOG_INFO("커맨드 처리 시작: ID={}, 타입={}", command.commandID, command.commandType);
            result = executor->execute(command);
            LOG_INFO("커맨드 처리 완료: ID={}, 결과={}",
                     result.commandID, result.resultStatus ? "성공" : "실패");
        }
        else
        {
            // 알 수 없는 타입일 경우
            result.resultStatus = 0; // 실패
            result.resultMessage = "알 수 없는 커맨드 타입";
        }
    }
    catch (const exception &e)
    {
        LOG_ERROR("커맨드 처리 중 오류 발생: {}", e.what());
        result.resultStatus = 0; // 실패
        result.resultMessage = string("오류: ") + e.what();
    }

    return result;
}