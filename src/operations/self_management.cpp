#include "operations/self_management.h"
#include "log/logger.h"
#include <cstdlib>

using namespace std;

namespace operations
{

    CommandResult SelfManagement::shutdown()
    {
        CommandResult result;

        LOG_INFO("프로그램 종료 작업 실행");

        // 프로그램 종료 로직 구현
        // exit(0); 실행 전에 필요한 정리 작업 수행

        result.resultStatus = 1; // 성공
        result.resultMessage = "프로그램 종료 명령 수행 완료";

        // 실제 종료는 이 함수 호출자가 결과를 처리하고 수행

        return result;
    }

    CommandResult SelfManagement::restart()
    {
        CommandResult result;

        LOG_INFO("프로그램 재시작 작업 실행");

        // 프로그램 재시작 로직 구현
        // 현재 실행 파일 경로 확인 및 새 프로세스 시작 후 현재 프로세스 종료

        result.resultStatus = 1; // 성공
        result.resultMessage = "프로그램 재시작 명령 수행 완료";

        // 실제 재시작은 이 함수 호출자가 결과를 처리하고 수행

        return result;
    }

} // namespace operations