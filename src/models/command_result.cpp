#include "models/command_result.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

string CommandResultUtils::toJson(const CommandResult& result) {
    json j;
    j["CommandID"] = result.commandID;
    j["NodeID"] = result.nodeID;
    j["CommandType"] = result.commandType;
    j["ResultStatus"] = result.resultStatus;
    j["ResultMessage"] = result.resultMessage;
    j["Target"] = result.target;
    
    return j.dump();
}

string CommandResultUtils::toJson(const vector<CommandResult>& results) {
    json j = json::array();
    for (const auto& result : results) {
        json item;
        item["CommandID"] = result.commandID;
        item["NodeID"] = result.nodeID;
        item["CommandType"] = result.commandType;
        item["ResultStatus"] = result.resultStatus;
        item["ResultMessage"] = result.resultMessage;
        item["Target"] = result.target;
        j.push_back(item);
    }
    return j.dump();
}

vector<CommandResult> CommandResultUtils::parseCommands(const string& jsonData) {
    vector<CommandResult> commands;
    try {
        json j = json::parse(jsonData);
        
        if (j.contains("commands") && j["commands"].is_array()) {
            for (const auto& cmd : j["commands"]) {
                CommandResult result;
                if (cmd.contains("CommandID")) result.commandID = cmd["CommandID"];
                if (cmd.contains("NodeID")) result.nodeID = cmd["NodeID"];
                if (cmd.contains("CommandType")) result.commandType = cmd["CommandType"];
                if (cmd.contains("CommandStatus")) result.resultStatus = cmd["CommandStatus"];
                if (cmd.contains("ResultMessage")) result.resultMessage = cmd["ResultMessage"];
                if (cmd.contains("Target")) result.target = cmd["Target"];
                
                commands.push_back(result);
            }
        }
    } catch (const exception& e) {
        // 파싱 에러 발생시 빈 벡터 반환
    }
    return commands;
}