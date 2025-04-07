#include "operations/system_control.h"
#include "log/logger.h"
#include <cstdlib>
#include <string>

using namespace std;

namespace operations
{

    CommandResult SystemControl::shutdownPC(int delaySeconds, CommandResult &command)
    {
        CommandResult result = command;
        LOG_INFO("PC 종료 작업 실행: 지연 시간 {}초", delaySeconds);

        //         string command = "shutdown -h +" + to_string(delaySeconds / 60);

        //         int exitCode = system(command.c_str());
        //         if (exitCode != 0)
        //         {
        //             result.resultStatus = 0; // 실패
        //             result.resultMessage = "PC 종료 명령 실패, 오류 코드: " + to_string(exitCode);
        //             return result;
        //         }

        result.resultStatus = 1; // 성공
        result.resultMessage = "PC 종료 명령 수행 완료";

        return result;
    }

    CommandResult SystemControl::restartPC(int delaySeconds, CommandResult &command)
    {
        CommandResult result = command;

        LOG_INFO("PC 재시작 작업 실행: 지연 시간 {}초", delaySeconds);

        // string command = "shutdown -r +" + to_string(delaySeconds / 60);

        //         int exitCode = system(command.c_str());
        //         if (exitCode != 0)
        //         {
        //             result.resultStatus = 0; // 실패
        //             result.resultMessage = "PC 재시작 명령 실패, 오류 코드: " + to_string(exitCode);
        //             return result;
        //         }

        result.resultStatus = 1; // 성공
        result.resultMessage = "PC 재시작 명령 수행 완료";

        return result;
    }

} // namespace operations