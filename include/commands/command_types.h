#pragma once

#include "models/command_result.h"
#include "commands/command_executor.h"

using namespace std;

/**
 * @brief A 타입 커맨드 실행기
 * 
 * A 타입 커맨드를 처리하기 위한 실행기 클래스입니다.
 */
class CommandTypeAExecutor : public ICommandExecutor {
public:
    /**
     * @brief A 타입 커맨드 실행 메서드
     * 
     * @param command 실행할 커맨드 정보
     * @return CommandResult 커맨드 실행 결과
     */
    CommandResult execute(const CommandResult& command) override;
};

/**
 * @brief B 타입 커맨드 실행기
 */
class CommandTypeBExecutor : public ICommandExecutor {
public:
    CommandResult execute(const CommandResult& command) override;
};

/**
 * @brief C 타입 커맨드 실행기
 */
class CommandTypeCExecutor : public ICommandExecutor {
public:
    CommandResult execute(const CommandResult& command) override;
};

/**
 * @brief D 타입 커맨드 실행기
 */
class CommandTypeDExecutor : public ICommandExecutor {
public:
    CommandResult execute(const CommandResult& command) override;
};

/**
 * @brief E 타입 커맨드 실행기
 */
class CommandTypeEExecutor : public ICommandExecutor {
public:
    CommandResult execute(const CommandResult& command) override;
}; 