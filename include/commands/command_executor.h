#pragma once

#include "models/command_result.h"
#include <memory>
#include <string>

using namespace std;

/**
 * @brief 커맨드 실행기 인터페이스
 *
 * 모든 타입의 커맨드 실행기가 구현해야 하는 기본 인터페이스를 정의합니다.
 */
class ICommandExecutor
{
public:
    virtual ~ICommandExecutor() = default;

    /**
     * @brief 커맨드 실행 메서드
     *
     * @param command 실행할 커맨드 정보
     * @return CommandResult 커맨드 실행 결과
     */
    virtual CommandResult execute(const CommandResult &command) = 0;
};

/**
 * @brief 커맨드 실행기 팩토리 클래스
 *
 * 커맨드 타입에 따라 적절한 실행기를 제공하는 팩토리 클래스입니다.
 */
class CommandExecutorFactory
{
public:
    /**
     * @brief 커맨드 타입에 맞는 실행기 반환
     *
     * @param commandType 커맨드 타입 (A, B, C, D, E 등)
     * @return ICommandExecutor* 실행기 포인터 (소유권 없음)
     */
    static ICommandExecutor *getExecutor(const string &commandType);

private:
    /**
     * @brief 초기화 여부 확인 함수
     *
     * @return true 초기화 완료
     * @return false 초기화 필요
     */
    static bool isInitialized();

    /**
     * @brief 실행기 초기화 함수
     */
    static void initialize();
};

/**
 * @brief 커맨드 프로세서 클래스
 *
 * 커맨드를 받아 적절한 실행기를 통해 처리하는 클래스입니다.
 */
class CommandProcessor
{
public:
    /**
     * @brief 커맨드 처리 메서드
     *
     * @param command 처리할 커맨드 정보
     * @return CommandResult 처리 결과
     */
    static CommandResult process(const CommandResult &command);
};