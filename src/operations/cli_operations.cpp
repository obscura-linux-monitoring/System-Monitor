#include "operations/cli_operations.h"
#include "log/logger.h"
#include <array>
#include <memory>
#include <sstream>

using namespace std;

namespace operations
{

    CommandResult CliOperations::executeCli(const string &command, CommandResult &result)
    {
        LOG_INFO("CLI 명령어 실행 중: {}", command);

        // popen을 사용하여 명령어 실행 및 출력 캡처
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "CLI 명령어 실행 실패: 파이프를 열 수 없음";
            return result;
        }

        // 명령어 출력 읽기
        array<char, 4096> buffer;
        stringstream output;

        size_t bytesRead;
        while ((bytesRead = fread(buffer.data(), 1, buffer.size() - 1, pipe)) > 0)
        {
            buffer[bytesRead] = '\0'; // 문자열 종료 보장
            output << buffer.data();
        }

        // 명령어 종료 상태 확인
        int exitCode = pclose(pipe);

        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "CLI 명령어 실행 실패, 오류 코드: " + to_string(exitCode) + "\n출력: " + output.str();
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = output.str();

        return result;
    }
} // namespace operations