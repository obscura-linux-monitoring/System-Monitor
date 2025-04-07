#include "operations/process_control.h"
#include "log/logger.h"
#include <cstdlib>
#include <string>

using namespace std;

namespace operations
{

    CommandResult ProcessControl::stopProcess(const string &processName)
    {
        CommandResult result;

        LOG_INFO("프로세스 정지 작업 실행: {}", processName);

        string command = "pkill " + processName;

        int exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "프로세스 정지 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "프로세스 정지 명령 수행 완료";

        return result;
    }

    CommandResult ProcessControl::killProcess(const string &processName)
    {
        CommandResult result;

        LOG_INFO("프로세스 강제 종료 작업 실행: {}", processName);

        string command = "pkill -9 " + processName;

        int exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "프로세스 강제 종료 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "프로세스 강제 종료 명령 수행 완료";

        return result;
    }

} // namespace operations