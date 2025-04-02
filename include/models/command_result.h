#pragma once

#include <string>
#include <vector>

using namespace std;

/**
 * @brief 커맨드 처리 결과를 저장하는 구조체
 */
struct CommandResult {
    int commandID;         ///< 커맨드 고유 ID
    string nodeID;         ///< 노드 ID
    string commandType;    ///< 커맨드 유형 (A, B, C 등)
    int resultStatus;      ///< 결과 상태 코드 (0: 실패, 1: 성공, 2: 진행 중)
    string resultMessage;  ///< 처리 결과 메시지
    string target;         ///< 커맨드 대상
    
    // 기본 생성자
    CommandResult() : commandID(0), resultStatus(0) {}
    
    // 매개변수가 있는 생성자
    CommandResult(int id, const string& node, const string& type, 
                 int status, const string& message, const string& tgt)
        : commandID(id), nodeID(node), commandType(type),
          resultStatus(status), resultMessage(message), target(tgt) {}
};

/**
 * @brief 커맨드 결과 모음을 관리하는 클래스
 */
class CommandResultUtils {
public:
    /**
     * @brief CommandResult 객체를 JSON 문자열로 변환
     * 
     * @param result 변환할 커맨드 결과 객체
     * @return JSON 문자열
     */
    static string toJson(const CommandResult& result);
    
    /**
     * @brief 커맨드 결과 모음을 JSON 문자열로 변환
     * 
     * @param results 변환할 커맨드 결과 배열
     * @return JSON 문자열
     */
    static string toJson(const vector<CommandResult>& results);
    
    /**
     * @brief JSON 문자열에서 커맨드 배열을 파싱
     * 
     * @param jsonData JSON 문자열
     * @return 파싱된 커맨드 배열
     */
    static vector<CommandResult> parseCommands(const string& jsonData);
};