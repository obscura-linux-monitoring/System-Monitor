#include "operations/service_control.h"
#include "log/logger.h"
#include <cstdlib>
#include <string>

using namespace std;

namespace operations
{

    CommandResult ServiceControl::startService(const string &serviceName)
    {
        CommandResult result;

        LOG_INFO("서비스 시작 작업 실행: {}", serviceName);
        string command = "sudo systemctl start " + serviceName;

        int exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "서비스 시작 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "서비스 시작 명령 수행 완료";

        return result;
    }

    CommandResult ServiceControl::stopService(const string &serviceName)
    {
        CommandResult result;

        LOG_INFO("서비스 정지 작업 실행: {}", serviceName);
        string command = "sudo systemctl stop " + serviceName;

        int exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "서비스 정지 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "서비스 정지 명령 수행 완료";

        return result;
    }

    CommandResult ServiceControl::restartService(const string &serviceName)
    {
        CommandResult result;

        LOG_INFO("서비스 재시작 작업 실행: {}", serviceName);

        string command = "sudo systemctl restart " + serviceName;

        int exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "서비스 재시작 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "서비스 재시작 명령 수행 완료";

        return result;
    }

    CommandResult ServiceControl::removeService(const string &serviceName)
    {
        CommandResult result;

        LOG_INFO("서비스 제거 작업 실행: {}", serviceName);

        string command = "sudo systemctl stop " + serviceName;

        int exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "서비스 제거 전 정지 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        command = "sudo systemctl disable " + serviceName;

        exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "서비스 제거 전 비활성화 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        command = "sudo rm -f /etc/systemd/system/" + serviceName + ".service";

        exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "서비스 제거 전 서비스 파일 삭제 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        if (daemonReload() == 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "데몬 재시작 명령 실패";
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "서비스 제거 작업 완료";

        return result;
    }

    int ServiceControl::daemonReload()
    {
        LOG_INFO("데몬 재시작 작업 실행");

        string command = "sudo systemctl daemon-reload";

        int exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            LOG_ERROR("데몬 재시작 명령 실패, 오류 코드: {}", exitCode);
            return 0;
        }

        return 1;
    }

} // namespace operations