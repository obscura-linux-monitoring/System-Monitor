#include "operations/process_control.h"
#include "log/logger.h"
#include <cstdlib>
#include <string>
#include "utils/process_utils.h"

using namespace std;

namespace operations
{
    CommandResult ProcessControl::killProcess(int pid, CommandResult &result)
    {
        LOG_INFO("프로세스 종료 작업 실행: {}", pid);

        string command = "kill -15 " + to_string(pid);

        int exitCode = system(command.c_str());
        if (exitCode != 0)
        {
            result.resultStatus = 0; // 실패
            result.resultMessage = "프로세스 종료 명령 실패, 오류 코드: " + to_string(exitCode);
            return result;
        }

        result.resultStatus = 1; // 성공
        result.resultMessage = "프로세스 종료 명령 수행 완료";

        return result;
    }

    CommandResult ProcessControl::restartProcess(int pid, CommandResult &result)
    {
        LOG_INFO("프로세스 재시작 작업 실행: {}", pid);
        
        // 프로세스가 서비스인지 확인
        string checkServiceCmd = "systemctl status $(ps -p " + to_string(pid) + " -o unit= 2>/dev/null) 2>/dev/null | grep -q '\\.service'";
        int isService = system(checkServiceCmd.c_str());
        
        if (isService == 0) {
            // 서비스인 경우
            LOG_INFO("PID {}는 시스템 서비스입니다. 서비스로 재시작합니다.", pid);
            
            // 서비스 이름 가져오기
            string getServiceCmd = "systemctl status $(ps -p " + to_string(pid) + " -o unit=) 2>/dev/null | grep '\\.service' | awk '{print $1}'";
            FILE* pipe = popen(getServiceCmd.c_str(), "r");
            if (!pipe) {
                result.resultStatus = 0;
                result.resultMessage = "서비스 이름을 가져오는 데 실패했습니다.";
                return result;
            }
            
            string serviceName = "";
            char buffer[4096]; // 버퍼 크기 증가
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                serviceName = buffer;
                // 개행 문자 제거
                serviceName.erase(remove(serviceName.begin(), serviceName.end(), '\n'), serviceName.end());
            }
            pclose(pipe);
            
            if (serviceName.empty()) {
                result.resultStatus = 0;
                result.resultMessage = "서비스 이름을 확인할 수 없습니다.";
                return result;
            }
            
            // 서비스 재시작
            string restartCmd = "systemctl restart " + serviceName;
            int exitCode = system(restartCmd.c_str());
            if (exitCode != 0) {
                result.resultStatus = 0;
                result.resultMessage = "서비스 재시작 명령 실패, 오류 코드: " + to_string(exitCode);
                return result;
            }
            
            // 서비스 재시작 확인 (5초 동안 대기)
            sleep(1); // 서비스 재시작 시작을 위한 약간의 대기
            if (!ProcessUtils::isProcessRunning(pid)) {
                // 기존 PID가 종료된 경우, 서비스가 다시 시작되었는지 확인
                string checkServiceRunningCmd = "systemctl is-active " + serviceName + " >/dev/null 2>&1";
                if (system(checkServiceRunningCmd.c_str()) != 0) {
                    result.resultStatus = 0;
                    result.resultMessage = "서비스 " + serviceName + "가 재시작되지 않았습니다.";
                    return result;
                }
            }
            
            result.resultStatus = 1;
            result.resultMessage = "서비스 " + serviceName + " 재시작 완료";
        } else {
            // 일반 프로세스인 경우
            LOG_INFO("PID {}는 일반 프로세스입니다. 명령어를 가져와 재시작합니다.", pid);
            
            // 프로세스 명령어 가져오기
            string getCmdCmd = "ps -p " + to_string(pid) + " -o cmd=";
            FILE* pipe = popen(getCmdCmd.c_str(), "r");
            if (!pipe) {
                result.resultStatus = 0;
                result.resultMessage = "프로세스 명령어를 가져오는 데 실패했습니다.";
                return result;
            }
            
            string cmd = "";
            char buffer[4096]; // 버퍼 크기 증가
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                cmd = buffer;
                // 개행 문자 제거
                cmd.erase(remove(cmd.begin(), cmd.end(), '\n'), cmd.end());
            }
            pclose(pipe);
            
            if (cmd.empty()) {
                result.resultStatus = 0;
                result.resultMessage = "프로세스 명령어를 확인할 수 없습니다.";
                return result;
            }
            
            // 명령어 이스케이프 처리
            string escapedCmd = ProcessUtils::escapeShellCommand(cmd);
            
            // 프로세스 종료
            string killCmd = "kill -15 " + to_string(pid);
            int exitCode = system(killCmd.c_str());
            if (exitCode != 0) {
                result.resultStatus = 0;
                result.resultMessage = "프로세스 종료 명령 실패, 오류 코드: " + to_string(exitCode);
                return result;
            }
            
            // 프로세스가 종료될 때까지 대기
            ProcessUtils::waitForProcessToEnd(pid, 10);
            
            // 프로세스 재시작
            string runCmd = escapedCmd + " &";
            exitCode = system(runCmd.c_str());
            if (exitCode != 0) {
                result.resultStatus = 0;
                result.resultMessage = "프로세스 재시작 명령 실패, 오류 코드: " + to_string(exitCode);
                return result;
            }
            
            // 프로세스가 실제로 재시작되었는지 확인 (새 PID 확인)
            sleep(1); // 프로세스 시작 대기
            string getPidCmd = "pgrep -f \"" + ProcessUtils::escapeShellCommand(cmd) + "\"";
            pipe = popen(getPidCmd.c_str(), "r");
            if (!pipe) {
                result.resultStatus = 0;
                result.resultMessage = "재시작된 프로세스 확인에 실패했습니다.";
                return result;
            }
            
            bool processFound = false;
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                processFound = true;
            }
            pclose(pipe);
            
            if (!processFound) {
                result.resultStatus = 0;
                result.resultMessage = "프로세스가 재시작되지 않았습니다.";
                return result;
            }
            
            result.resultStatus = 1;
            result.resultMessage = "프로세스 재시작 명령 수행 완료";
        }
        
        return result;
    }
}