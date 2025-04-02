#include "network/client/data_sender.h"
#include "log/logger.h"
#include <iostream>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// main.cpp에서 선언된 전역 변수 참조
extern atomic<bool> running;

/**
 * @brief DataSender 클래스 생성자
 * 
 * @param serverInfo 연결할 서버 정보 (주소, 포트)
 * @param dataQueue 전송할 시스템 메트릭 데이터를 저장하는 스레드 안전 큐
 * @param user_id 사용자 식별자
 */
DataSender::DataSender(const ServerInfo &serverInfo, ThreadSafeQueue<SystemMetrics> &dataQueue, const string &user_id)
    : serverInfo_(serverInfo), dataQueue_(dataQueue), user_id_(user_id), isConnected_(false), running_(false),
      commandResultQueue_(100), commandQueue_(100), numWorkers_(4) // 커맨드 결과 큐와 커맨드 큐 크기 설정
{
    // WebSocket 클라이언트 초기화
    client_.init_asio();
    
    // 로깅 비활성화
    client_.clear_access_channels(websocketpp::log::alevel::all);
    client_.clear_error_channels(websocketpp::log::elevel::all);
    
    // 콜백 설정
    client_.set_message_handler(
        [this](WebsocketHandle hdl, WebsocketClient::message_ptr msg)
        {
            this->handleMessage(hdl, msg);
        });
}

/**
 * @brief DataSender 클래스 소멸자
 * 
 * 데이터 전송을 중지하고 서버와의 연결을 종료합니다.
 */
DataSender::~DataSender()
{
    stopSending();
    disconnect();
}

/**
 * @brief 서버에 WebSocket 연결을 시도합니다.
 * 
 * @return 연결 성공 시 true, 실패 시 false 반환
 */
bool DataSender::connect()
{
    // URI에 '/ws' 경로 추가
    string uri = "ws://" + serverInfo_.address + ":" +
                 to_string(serverInfo_.port) + "/ws";
    
    try
    {
        websocketpp::lib::error_code ec;
        WebsocketClient::connection_ptr con = client_.get_connection(uri, ec);
        
        if (ec)
        {
            return false;
        }
        
        // 연결 핸들 저장
        connectionHandle_ = con->get_handle();
        
        // 연결 실패 핸들러 설정
        client_.set_fail_handler([this](WebsocketHandle hdl)
                                 { isConnected_ = false; });
        
        // 연결 종료 핸들러 설정
        client_.set_close_handler([this](WebsocketHandle hdl)
                                  { isConnected_ = false; });
        
        // 연결 시도
        client_.connect(con);
        
        // 비동기로 실행 (스레드 저장)
        clientThread_ = thread([this]()
                               { this->client_.run(); });
        
        isConnected_ = true;
        
        // 커맨드 처리 스레드 시작
        commandThread_ = thread(&DataSender::commandProcessor, this);
        
        // 워커 스레드 풀 생성
        for (int i = 0; i < numWorkers_; i++) {
            workerThreads_.push_back(thread(&DataSender::workerThread, this));
        }
        
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * @brief 서버와의 WebSocket 연결을 종료합니다.
 * 
 * 연결이 활성화된 경우 정상적으로 종료하고 관련 리소스를 정리합니다.
 */
void DataSender::disconnect()
{
    if (isConnected_)
    {
        try
        {
            // 먼저 연결을 정상적으로 종료
            websocketpp::lib::error_code ec;
            client_.close(connectionHandle_, websocketpp::close::status::normal, "정상 종료", ec);
            
            // 잠시 대기하여 종료 메시지가 전송될 시간을 줌
            this_thread::sleep_for(chrono::milliseconds(100));
            
            // asio 서비스 중지
            client_.stop();
            
            if (clientThread_.joinable())
            {
                clientThread_.join();
            }
            
            // 커맨드 처리 스레드 종료 대기
            if (commandThread_.joinable())
            {
                commandThread_.join();
            }
            
            // 워커 스레드 풀 종료 대기
            for (auto& worker : workerThreads_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            workerThreads_.clear();
            
            isConnected_ = false;
            LOG_INFO("WebSocket 연결이 정상적으로 종료되었습니다.");
        }
        catch (const exception &e)
        {
            LOG_ERROR("WebSocket 연결 종료 중 오류 발생: {}", e.what());
        }
    }
}

/**
 * @brief 데이터 전송 작업을 시작합니다.
 * 
 * @param intervalSeconds 데이터 전송 간격(초)
 */
void DataSender::startSending(int intervalSeconds)
{
    if (running_)
        return;
    
    running_ = true;
    senderThread_ = thread(&DataSender::sendLoop, this, intervalSeconds);
}

/**
 * @brief 데이터 전송 작업을 중지합니다.
 * 
 * 실행 중인 전송 스레드를 안전하게 종료합니다.
 */
void DataSender::stopSending()
{
    if (!running_)
        return;
    
    running_ = false;
    
    if (senderThread_.joinable())
    {
        senderThread_.join();
    }
}

/**
 * @brief 데이터 전송 루프를 실행하는 메서드
 * 
 * 지정된 간격으로 큐에서 시스템 메트릭 데이터를 가져와 서버로 전송합니다.
 * 
 * @param intervalSeconds 전송 간격(초)
 */
void DataSender::sendLoop(int intervalSeconds)
{
    while (running_ && isConnected_)
    {
        auto startTime = chrono::steady_clock::now();
        
        // 큐에서 데이터 가져와서 전송
        SystemMetrics metrics;
        if (dataQueue_.try_pop(metrics, chrono::milliseconds(100)))
        {
            metrics.user_id = user_id_;
            sendMetrics(metrics);
        }
        
        // 송신 간격 조절
        auto endTime = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        auto sleepTime = chrono::seconds(intervalSeconds) - elapsed;
        
        if (sleepTime > chrono::milliseconds(0))
        {
            this_thread::sleep_for(sleepTime);
        }
    }
}

/**
 * @brief 시스템 메트릭 데이터를 서버로 전송합니다.
 * 
 * 시스템 메트릭 데이터를 JSON 형식으로 변환하여 WebSocket을 통해 서버로 전송합니다.
 * 전송 시작/종료 시간 및 소요 시간을 기록하고 로그로 출력합니다.
 * 커맨드 처리 결과가 있다면 함께 전송합니다.
 * 
 * @param metrics 전송할 시스템 메트릭 데이터
 * @return 전송 성공 시 true, 실패 시 false 반환
 */
bool DataSender::sendMetrics(const SystemMetrics &metrics)
{
    if (!isConnected_)
        return false;
    
    // 전송 시작 시간 기록
    auto sendStartTime = chrono::steady_clock::now();
    time_t now = time(nullptr);
    char startTimestamp[30];
    strftime(startTimestamp, sizeof(startTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // 메트릭을 JSON으로 변환
    string metricsJson = SystemMetricsUtil::toJson(metrics);
    
    // JSON 파싱 후 명령 처리 결과 추가
    json metricsObj;
    try {
        metricsObj = json::parse(metricsJson);
        
        // 커맨드 결과 큐에서 데이터 가져오기
        vector<CommandResult> commandResults;
        CommandResult result;
        while (commandResultQueue_.try_pop(result, chrono::milliseconds(100))) {
            commandResults.push_back(result);
        }
        
        // 커맨드 결과가 있으면 JSON에 추가
        if (!commandResults.empty()) {
            json commandResultsArray = json::array();
            for (const auto& cmd : commandResults) {
                json cmdJson;
                cmdJson["command_id"] = cmd.commandID;
                cmdJson["node_id"] = cmd.nodeID;
                cmdJson["command_type"] = cmd.commandType;
                cmdJson["result_status"] = cmd.resultStatus;
                cmdJson["result_message"] = cmd.resultMessage;
                cmdJson["target"] = cmd.target;
                commandResultsArray.push_back(cmdJson);
            }
            metricsObj["command_results"] = commandResultsArray;
        }
    } catch (const exception& e) {
        LOG_ERROR("JSON 파싱 중 오류 발생: {}", e.what());
        return false;
    }
    
    // 최종 JSON을 문자열로 변환
    string data = metricsObj.dump();
    
    // WebSocket으로 전송
    websocketpp::lib::error_code ec;
    client_.send(connectionHandle_, data, websocketpp::frame::opcode::text, ec);
    
    // 전송 종료 시간 기록 및 소요 시간 계산
    auto sendEndTime = chrono::steady_clock::now();
    auto sendDuration = chrono::duration_cast<chrono::milliseconds>(sendEndTime - sendStartTime);
    
    // 전송 시간 정보 출력
    char endTimestamp[30];
    time_t endTime = time(nullptr);
    strftime(endTimestamp, sizeof(endTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&endTime));
    
    string logMessage = "[전송] 시작: " + string(startTimestamp) +
                        ", 종료: " + string(endTimestamp) +
                        ", 소요 시간: " + to_string(sendDuration.count()) + "ms, 데이터 크기: " +
                        to_string(data.length()) + "바이트" +
                        (ec ? ", 오류 발생: " + ec.message() : ", 성공");
    
    if (ec)
    {
        LOG_ERROR(logMessage);
    }
    else
    {
        LOG_INFO(logMessage);
    }
    
    return !ec;
}

/**
 * @brief 서버로부터 수신된 메시지를 처리하는 핸들러
 * 
 * @param hdl WebSocket 연결 핸들
 * @param msg 수신된 메시지 포인터
 */
void DataSender::handleMessage(WebsocketHandle hdl, WebsocketClient::message_ptr msg)
{
    (void)hdl;
    // 서버로부터의 메시지 처리
    string data = msg->get_payload();
    
    try
    {
        LOG_INFO("메시지 수신: {}", data);
        
        // JSON 파싱
        json messageJson = json::parse(data);
        
        // 커맨드 목록 확인
        if (messageJson.contains("commands") && messageJson["commands"].is_array()) {
            // 파싱된 커맨드를 처리 큐에 추가
            for (const auto& cmd : messageJson["commands"]) {
                CommandResult command;
                if (cmd.contains("CommandID")) command.commandID = cmd["CommandID"];
                if (cmd.contains("NodeID")) command.nodeID = cmd["NodeID"];
                if (cmd.contains("CommandType")) command.commandType = cmd["CommandType"];
                if (cmd.contains("CommandStatus")) command.resultStatus = cmd["CommandStatus"];
                if (cmd.contains("Target")) command.target = cmd["Target"];
                
                // 수신한 커맨드 정보 로깅
                LOG_INFO("커맨드 수신: ID={}, 타입={}, 대상={}", 
                         command.commandID, command.commandType, command.target);
                
                // 비동기적 처리를 위해 커맨드를 작업 큐에 추가
                commandQueue_.push(command);
            }
        }
    }
    catch (const exception &e)
    {
        LOG_ERROR("메시지 처리 중 오류 발생: {}", e.what());
    }
}

/**
 * @brief 커맨드 처리 쓰레드 함수
 * 
 * 워커 스레드 풀이 있는 경우 커맨드 큐와 결과 큐를 모니터링하고
 * 필요한 상태 보고 및 관리 작업을 수행합니다.
 */
void DataSender::commandProcessor()
{
    LOG_INFO("커맨드 처리기 시작");
    
    while (running.load() && isConnected_) {
        // 주기적으로 큐 상태 확인 및 보고
        LOG_DEBUG("커맨드 처리 상태: 대기 중인 작업={}, 완료된 결과={}",
                 commandQueue_.size(), commandResultQueue_.size());
        
        this_thread::sleep_for(chrono::seconds(1));
    }
    
    LOG_INFO("커맨드 처리기 종료");
}

/**
 * @brief 커맨드를 처리하는 함수
 * 
 * @param command 처리할 커맨드
 * @return 처리 결과
 */
CommandResult DataSender::processCommand(const CommandResult& command)
{
    CommandResult result = command;
    
    try {
        // 커맨드 타입에 따른 처리 로직
        if (command.commandType == "A") {
            // A 타입 커맨드 처리 로직
            LOG_INFO("A 타입 커맨드 처리 시작: ID={}", command.commandID);
            // 여기에 실제 처리 로직 구현
            
            result.resultStatus = 1; // 성공
            result.resultMessage = "A 타입 커맨드 처리 완료";
        }
        else if (command.commandType == "B") {
            // B 타입 커맨드 처리 로직
            LOG_INFO("B 타입 커맨드 처리 시작: ID={}", command.commandID);
            // 여기에 실제 처리 로직 구현
            
            result.resultStatus = 1; // 성공
            result.resultMessage = "B 타입 커맨드 처리 완료";
        }
        else {
            // 알 수 없는 타입
            LOG_WARN("알 수 없는 커맨드 타입: {}", command.commandType);
            result.resultStatus = 0; // 실패
            result.resultMessage = "알 수 없는 커맨드 타입";
        }
        
        LOG_INFO("커맨드 처리 완료: ID={}, 결과={}", 
                 command.commandID, result.resultStatus ? "성공" : "실패");
    }
    catch (const exception& e) {
        LOG_ERROR("커맨드 처리 중 오류 발생: {}", e.what());
        result.resultStatus = 0; // 실패
        result.resultMessage = string("오류: ") + e.what();
    }
    
    return result;
}

/**
 * @brief 워커 스레드 함수
 * 
 * 커맨드 큐에서 작업을 가져와 처리하는 함수
 */
void DataSender::workerThread()
{
    while (running.load() && isConnected_) {
        // 커맨드 큐에서 처리할 작업 가져오기
        CommandResult command;
        if (commandQueue_.try_pop(command, chrono::milliseconds(100))) {
            // 커맨드 처리 시작 로깅
            LOG_INFO("워커: 커맨드 처리 시작: ID={}, 타입={}", command.commandID, command.commandType);
            
            // 커맨드 처리 및 결과 저장
            CommandResult result = processCommand(command);
            
            // 처리 결과를 결과 큐에 저장
            commandResultQueue_.push(result);
            
            LOG_INFO("워커: 커맨드 처리 완료: ID={}, 결과={}", 
                    result.commandID, result.resultStatus ? "성공" : "실패");
        }
    }
}
