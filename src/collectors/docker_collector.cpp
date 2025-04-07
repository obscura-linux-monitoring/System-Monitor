#include "collectors/docker_collector.h"

#include <curl/curl.h>
#include <string>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <thread>
#include <future>
#include <vector>
#include <mutex>
#include <chrono>
#include <iostream>
#include "log/logger.h"

/**
 * @class DockerCollector
 * @brief Docker 컨테이너 정보 수집을 담당하는 클래스
 *
 * Docker API를 사용하여 실행 중인 모든 컨테이너의 상세 정보와 리소스 사용량을 수집합니다.
 * 성능 최적화를 위해 curl 핸들 풀링, 비동기 처리, 결과 캐싱 기법을 사용합니다.
 * Docker 유닉스 소켓(/var/run/docker.sock)에 연결하여 정보를 수집합니다.
 */

using namespace std;
using json = nlohmann::json;

/**
 * @brief Docker API로부터 데이터를 수신하기 위한 콜백 함수
 *
 * @param contents 수신된 데이터
 * @param size 각 데이터 항목의 크기
 * @param nmemb 데이터 항목의 개수
 * @param userp 사용자 제공 데이터 포인터 (일반적으로 결과를 저장할 문자열)
 * @return 처리된 바이트 수
 */
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

/**
 * @brief DockerCollector 클래스의 생성자
 *
 * CURL을 글로벌하게 초기화하고 curl 핸들 풀을 설정합니다.
 */
DockerCollector::DockerCollector()
{
    // CURL 글로벌 초기화 - 프로그램 시작시 한 번만 호출
    curl_global_init(CURL_GLOBAL_ALL);

    // curl 핸들 풀 초기화
    initCurlPool();
}

/**
 * @brief DockerCollector 클래스의 소멸자
 *
 * 모든 curl 핸들을 정리하고 CURL 글로벌 리소스를 해제합니다.
 */
DockerCollector::~DockerCollector()
{
    // 핸들 풀 정리
    for (auto handle : curlHandlePool)
    {
        curl_easy_cleanup(handle);
    }
    curlHandlePool.clear();

    // 프로그램 종료시 정리
    curl_global_cleanup();
}

/**
 * @brief curl 핸들 풀을 초기화합니다
 *
 * 지정된 수의 curl 핸들을 생성하고 기본 옵션을 설정합니다.
 *
 * @param poolSize 생성할 curl 핸들의 수
 */
void DockerCollector::initCurlPool(size_t poolSize)
{
    for (size_t i = 0; i < poolSize; i++)
    {
        CURL *handle = curl_easy_init();
        // 기본 옵션 설정
        curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(handle, CURLOPT_FORBID_REUSE, 0L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 2L); // 2초 타임아웃
        curlHandlePool.push_back(handle);
    }
}

/**
 * @brief 풀에서 curl 핸들을 획득합니다
 *
 * 사용 가능한 핸들이 없으면 새로운 핸들을 생성합니다.
 *
 * @return CURL* 사용할 준비가 된 curl 핸들, 실패 시 nullptr
 */
CURL *DockerCollector::getCurlHandle()
{
    lock_guard<mutex> lock(curlPoolMutex);
    if (curlHandlePool.empty())
    {
        CURL *handle = curl_easy_init();
        if (!handle)
        {
            cerr << "심각한 오류: curl 핸들을 초기화할 수 없습니다." << endl;
            return nullptr; // 오류 처리 필요
        }
        // 기본 옵션 설정
        curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(handle, CURLOPT_FORBID_REUSE, 0L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 2L);
        return handle;
    }
    CURL *handle = curlHandlePool.back();
    curlHandlePool.pop_back();
    return handle;
}

/**
 * @brief 사용한 curl 핸들을 풀에 반환합니다
 *
 * 핸들을 초기화하고 기본 옵션을 다시 설정한 후 풀에 추가합니다.
 *
 * @param handle 반환할 curl 핸들
 */
void DockerCollector::returnCurlHandle(CURL *handle)
{
    lock_guard<mutex> lock(curlPoolMutex);
    curl_easy_reset(handle); // 핸들 상태 초기화
    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(handle, CURLOPT_FORBID_REUSE, 0L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 2L);
    curlHandlePool.push_back(handle);
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

/**
 * @brief 컨테이너의 포트 정보를 포트 번호 기준으로 정렬합니다
 *
 * @param container 포트를 정렬할 컨테이너 정보 구조체
 */
void DockerCollector::sortPorts(DockerContainerInfo &container)
{
    sort(container.container_ports.begin(), container.container_ports.end(), [](const DockerContainerPort &a, const DockerContainerPort &b)
         { return a.container_port < b.container_port; });
}

/**
 * @brief Docker API를 통해 컨테이너 정보를 수집합니다
 *
 * Docker 소켓에 연결하여 실행 중인 모든 컨테이너의 정보를 가져옵니다.
 * 컨테이너별로 상세 정보, 통계, 네트워크, 볼륨 등의 데이터를 수집합니다.
 * 성능 최적화를 위해 캐싱 및 비동기 처리를 활용합니다.
 */
void DockerCollector::collect()
{
    CURL *curl = getCurlHandle();
    string readBuffer;

    if (curl)
    {
        // Docker 소켓에 연결
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/json");
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK)
        {
            returnCurlHandle(curl);
            return;
        }

        // JSON 파싱
        vector<DockerContainerInfo> localcontainers;
        try
        {
            auto root = json::parse(readBuffer);

            // 병렬 처리를 위한 준비
            vector<future<DockerContainerInfo>> futures;

            // 현재 시간 - 캐시 유효성 검사용
            auto now = chrono::system_clock::now();

            for (const auto &container : root)
            {
                // 컨테이너 ID 가져오기
                string containerId = container["Id"];

                // 캐시 확인
                bool useCache = false;
                DockerContainerInfo cachedInfo;
                {
                    lock_guard<mutex> lock(containersMutex);
                    if (statsCache.find(containerId) != statsCache.end() &&
                        chrono::duration_cast<chrono::seconds>(now - statsCache[containerId].timestamp).count() < cacheTTLSeconds)
                    {
                        cachedInfo = statsCache[containerId].info;
                        useCache = true;
                    }
                }

                if (useCache)
                {
                    // 캐시된 정보 사용
                    futures.push_back(async(launch::async, [cachedInfo]()
                                            { return cachedInfo; }));
                    continue;
                }

                // 컨테이너 정보 처리를 비동기로 실행
                futures.push_back(async(launch::async, [this, container, containerId, now]()
                                        {
                    try
                    {
                        DockerContainerInfo info;
                        
                        // 컨테이너 ID 설정
                        info.container_id = containerId;

                        // Names는 배열로 제공됨
                        if (container.contains("Names") && container["Names"].is_array() && !container["Names"].empty())
                        {
                            info.container_name = container["Names"][0];
                            
                            // 슬래시(/) 제거
                            if (info.container_name[0] == '/')
                                info.container_name = info.container_name.substr(1);
                        }
                        else
                        {
                            info.container_name = "Unknown";
                            
                        }

                        // Image 필드 확인
                        if (container.contains("Image") && container["Image"].is_string())
                        {
                            info.container_image = container["Image"];
                            
                        }

                        // Status 필드 확인
                        if (container.contains("Status") && container["Status"].is_string())
                        {
                            info.container_status = container["Status"];
                            
                        }

                        // Created 필드 확인
                        if (container.contains("Created") && container["Created"].is_number_integer())
                        {
                            time_t timestamp = container["Created"];
                            char timeStr[20];
                            struct tm *timeinfo = localtime(&timestamp);
                            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
                            info.container_created = string(timeStr);
                            
                        }

                        // State 필드 확인
                        if (container.contains("State") && container["State"].is_string())
                        {
                            info.container_health.status = container["State"];
                            
                        }

                        // PortBindings 처리
                        if (container.contains("Ports") && container["Ports"].is_array())
                        {
                            
                            for (const auto &port : container["Ports"])
                            {
                                DockerContainerPort docker_port;
                                if (port.contains("PrivatePort"))
                                {
                                    docker_port.container_port = port["PrivatePort"].dump();
                                    
                                    if (port.contains("PublicPort"))
                                    {
                                        docker_port.host_port = port["PublicPort"].dump();
                                        
                                    }
                                    if (port.contains("Type"))
                                    {
                                        docker_port.protocol = port["Type"];
                                        
                                    }
                                }
                                info.container_ports.push_back(docker_port);
                            }
                        }

                        // Command 처리
                        if (container.contains("Command") && container["Command"].is_string())
                        {
                            info.command = container["Command"];
                            
                        }
                        else if (container.contains("Cmd") && container["Cmd"].is_array())
                        {
                            stringstream cmd;
                            bool first = true;
                            for (const auto &arg : container["Cmd"])
                            {
                                if (!first)
                                    cmd << " ";
                                cmd << arg.get<string>();
                                first = false;
                            }
                            info.command = cmd.str();
                            
                        }
                        
                        // 컨테이너 상세 정보 가져오기 (inspect API 사용)
                        string inspectBuffer;
                        CURL *inspectCurl = this->getCurlHandle();
                        if (inspectCurl)
                        {
                            string inspectUrl = "http://localhost/containers/" + containerId + "/json";
                            curl_easy_setopt(inspectCurl, CURLOPT_URL, inspectUrl.c_str());
                            curl_easy_setopt(inspectCurl, CURLOPT_WRITEDATA, &inspectBuffer);
                            
                            CURLcode inspectRes = curl_easy_perform(inspectCurl);
                            this->returnCurlHandle(inspectCurl);
                            
                            if (inspectRes == CURLE_OK)
                            {
                                auto inspectRoot = json::parse(inspectBuffer);
                                
                                // 컨테이너 상태 가져오기
                                const auto& state = inspectRoot["State"];
                                
                                // 재시작 횟수 가져오기
                                if (state.contains("RestartCount") && state["RestartCount"].is_number_integer())
                                {
                                    info.restarts = state["RestartCount"];
                                    
                                }
                                
                                // 건강 상태 정보 가져오기
                                if (state.contains("Health"))
                                {
                                    const auto& health = state["Health"];
                                    if (health.contains("Status") && health["Status"].is_string())
                                    {
                                        info.container_health.status = health["Status"];
                                        
                                    }
                                    
                                    if (health.contains("FailingStreak") && health["FailingStreak"].is_number_integer())
                                    {
                                        info.container_health.failing_streak = health["FailingStreak"];
                                        
                                    }
                                    
                                    // 마지막 건강 체크 결과
                                    if (health.contains("Log") && health["Log"].is_array() && !health["Log"].empty())
                                    {
                                        const auto& lastLog = health["Log"][health["Log"].size() - 1];
                                        if (lastLog.contains("Output") && lastLog["Output"].is_string())
                                        {
                                            info.container_health.last_check_output = lastLog["Output"];
                                            
                                        }
                                    }
                                }
                                
                                // 네트워크 정보 가져오기
                                if (inspectRoot.contains("NetworkSettings") && 
                                    inspectRoot["NetworkSettings"].contains("Networks") &&
                                    inspectRoot["NetworkSettings"]["Networks"].is_object())
                                {
                                    const auto& networks = inspectRoot["NetworkSettings"]["Networks"];
                                    for (auto it = networks.begin(); it != networks.end(); ++it)
                                    {
                                        string networkName = it.key();
                                        const auto& network = it.value();
                                        DockerContainerNetwork netInfo;
                                        
                                        netInfo.network_name = networkName;
                                        
                                        if (network.contains("IPAddress") && network["IPAddress"].is_string())
                                        {
                                            netInfo.network_ip = network["IPAddress"];
                                            
                                        }
                                        
                                        if (network.contains("MacAddress") && network["MacAddress"].is_string())
                                        {
                                            netInfo.network_mac = network["MacAddress"];
                                            
                                        }
                                        
                                        info.container_network.push_back(netInfo);
                                    }
                                }
                                
                                // 볼륨 정보 가져오기
                                if (inspectRoot.contains("Mounts") && inspectRoot["Mounts"].is_array())
                                {
                                    for (const auto& mount : inspectRoot["Mounts"])
                                    {
                                        DockerContainerVolume volume;
                                        
                                        if (mount.contains("Source") && mount["Source"].is_string())
                                        {
                                            volume.source = mount["Source"];
                                            
                                        }
                                        
                                        if (mount.contains("Destination") && mount["Destination"].is_string())
                                        {
                                            volume.destination = mount["Destination"];
                                            
                                        }
                                        
                                        if (mount.contains("Mode") && mount["Mode"].is_string())
                                        {
                                            volume.mode = mount["Mode"];
                                            
                                        }
                                        
                                        if (mount.contains("Type") && mount["Type"].is_string())
                                        {
                                            volume.type = mount["Type"];
                                            
                                        }
                                        
                                        info.container_volumes.push_back(volume);
                                        
                                    }
                                }
                                
                                // 환경 변수 정보 가져오기
                                if (inspectRoot.contains("Config") && 
                                    inspectRoot["Config"].contains("Env") && 
                                    inspectRoot["Config"]["Env"].is_array())
                                {
                                    for (const auto& env : inspectRoot["Config"]["Env"])
                                    {
                                        
                                        if (env.is_string())
                                        {
                                            string envStr = env;
                                            size_t pos = envStr.find('=');
                                            
                                            if (pos != string::npos)
                                            {
                                                DockerContainerEnv envInfo;
                                                envInfo.key = envStr.substr(0, pos);
                                                envInfo.value = envStr.substr(pos + 1);
                                                info.container_env.push_back(envInfo);
                                                
                                                
                                            }
                                        }
                                    }
                                }

                                // 라벨 정보 수집
                                if (container.contains("Labels") && container["Labels"].is_object())
                                {
                                    for (auto it = container["Labels"].begin(); it != container["Labels"].end(); ++it)
                                    {
                                        DockerContainerLabel label;
                                        label.label_key = it.key();
                                        label.label_value = it.value();
                                        info.labels.push_back(label);
                                        
                                        
                                    }
                                }

                                // 또는 inspect API에서 더 자세한 레이블 정보를 가져올 수도 있음
                                if (inspectRoot.contains("Config") && 
                                    inspectRoot["Config"].contains("Labels") && 
                                    inspectRoot["Config"]["Labels"].is_object())
                                {
                                    const auto& labels = inspectRoot["Config"]["Labels"];
                                    for (auto it = labels.begin(); it != labels.end(); ++it)
                                    {
                                        string label_name = it.key();
                                        // 중복 방지를 위한 체크
                                        bool found = false;
                                        for (const auto &existing : info.labels)
                                        {
                                            if (existing.label_key == label_name)
                                            {
                                                found = true;
                                                break;
                                            }
                                        }
                                        
                                        if (!found)
                                        {
                                            DockerContainerLabel label;
                                            label.label_key = label_name;
                                            label.label_value = it.value();
                                            info.labels.push_back(label);
                                        }
                                    }
                                }
                            }
                        }
                        
                        // 통계 정보 가져오기
                        string statsBuffer;
                        CURL *statsCurl = this->getCurlHandle();
                        if (statsCurl)
                        {
                            string containerName;
                            if (container.contains("Names") && container["Names"].is_array() && !container["Names"].empty()) {
                                containerName = container["Names"][0];
                            }
                            
                            // 최적화된 API 요청 URL
                            string statsUrl = "http://localhost/containers/" + containerId + "/stats?stream=false&one-shot=true";
                            curl_easy_setopt(statsCurl, CURLOPT_URL, statsUrl.c_str());
                            curl_easy_setopt(statsCurl, CURLOPT_WRITEDATA, &statsBuffer);
                            
                            CURLcode statsRes = curl_easy_perform(statsCurl);
                            
                            this -> returnCurlHandle(statsCurl);
                            
                            if (statsRes == CURLE_OK)
                            {
                                auto statsRoot = json::parse(statsBuffer);

                                // CPU 사용량 계산
                                uint64_t cpuDelta = statsRoot["cpu_stats"]["cpu_usage"]["total_usage"].get<uint64_t>() -
                                                    statsRoot["precpu_stats"]["cpu_usage"]["total_usage"].get<uint64_t>();
                                

                                // 시스템 CPU 사용량을 안전하게 처리
                                uint64_t systemDelta = 0;
                                if (statsRoot["cpu_stats"].contains("system_cpu_usage") && 
                                    !statsRoot["cpu_stats"]["system_cpu_usage"].is_null() &&
                                    statsRoot["precpu_stats"].contains("system_cpu_usage") && 
                                    !statsRoot["precpu_stats"]["system_cpu_usage"].is_null()) {
                                    systemDelta = statsRoot["cpu_stats"]["system_cpu_usage"].get<uint64_t>() -
                                                      statsRoot["precpu_stats"]["system_cpu_usage"].get<uint64_t>();
                                }

                                // CPU 개수를 안전하게 처리
                                int numCPUs = 0;
                                if (statsRoot["cpu_stats"].contains("online_cpus") && 
                                    !statsRoot["cpu_stats"]["online_cpus"].is_null()) {
                                    numCPUs = statsRoot["cpu_stats"]["online_cpus"].get<int>();
                                }

                                if (systemDelta > 0 && numCPUs > 0)
                                {
                                    info.cpu_usage = (static_cast<double>(cpuDelta) / static_cast<double>(systemDelta)) * numCPUs * 100.0;
                                }

                                // 메모리 사용량 (MB)
                                info.memory_usage = statsRoot["memory_stats"]["usage"].get<uint64_t>();
                                
                                // 네트워크 I/O
                                const auto &networks = statsRoot["networks"];
                                info.network_rx_bytes = 0;
                                info.network_tx_bytes = 0;
                                for (auto it = networks.begin(); it != networks.end(); ++it)
                                {
                                    info.network_rx_bytes += it.value()["rx_bytes"].get<uint64_t>();
                                    info.network_tx_bytes += it.value()["tx_bytes"].get<uint64_t>();
                                }

                                // 블록 I/O
                                const auto &blkio = statsRoot["blkio_stats"]["io_service_bytes_recursive"];
                                for (const auto &io : blkio)
                                {
                                    string op = io["op"];
                                    if (op == "Read")
                                        info.block_read = io["value"].get<uint64_t>();
                                    else if (op == "Write")
                                        info.block_write = io["value"].get<uint64_t>();
                                }
                                
                                
                                // 메모리 제한 및 백분율 추가
                                if (statsRoot["memory_stats"].contains("limit") && statsRoot["memory_stats"]["limit"].is_number_unsigned())
                                {
                                    info.memory_limit = statsRoot["memory_stats"]["limit"].get<uint64_t>();
                                    
                                    // 메모리 사용량 백분율 계산
                                    if (info.memory_limit > 0)
                                    {
                                        info.memory_percent = (static_cast<double>(info.memory_usage) / static_cast<double>(info.memory_limit)) * 100.0;
                                    }
                                }
                                
                                // PID 개수 추가
                                if (statsRoot.contains("pids_stats") && 
                                    statsRoot["pids_stats"].contains("current") && 
                                    statsRoot["pids_stats"]["current"].is_number_integer())
                                {
                                    info.pids = statsRoot["pids_stats"]["current"].get<int>();
                                }
                                
                                // 네트워크 인터페이스별 정보 업데이트
                                if (statsRoot.contains("networks") && statsRoot["networks"].is_object())
                                {
                                    const auto& networkList = statsRoot["networks"];
                                    for (auto it = networkList.begin(); it != networkList.end(); ++it)
                                    {
                                        string netName = it.key();
                                        // 기존 네트워크 목록에서 일치하는 네트워크 찾기
                                        for (auto& netInfo : info.container_network)
                                        {
                                            if (netInfo.network_name == netName)
                                            {
                                                netInfo.network_rx_bytes = to_string(it.value()["rx_bytes"].get<uint64_t>());
                                                netInfo.network_tx_bytes = to_string(it.value()["tx_bytes"].get<uint64_t>());
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        sortPorts(info);
                        
                        // 캐시에 정보 저장
                        {
                            lock_guard<mutex> lock(containersMutex);
                            statsCache[containerId] = {now, info};
                            
                        }
                        
                        return info;
                    }
                    catch (const json::exception &e)
                    {
                        cerr << "JSON 예외 발생: " << e.what() << endl;
                        return DockerContainerInfo(); // 빈 컨테이너 정보 반환
                    }
                    catch (const exception &e)
                    {
                        cerr << "예외 발생: " << e.what() << endl;
                        return DockerContainerInfo(); // 빈 컨테이너 정보 반환
                    } }));
            }

            // 모든 비동기 작업 결과 수집
            for (auto &f : futures)
            {
                DockerContainerInfo info = f.get();
                if (!info.container_name.empty() && info.container_name != "Unknown") // 유효한 컨테이너 정보만 추가
                {
                    localcontainers.push_back(info);
                }
            }
        }
        catch (const json::exception &e)
        {
            returnCurlHandle(curl);
            return;
        }
        catch (const exception &e)
        {
            returnCurlHandle(curl);
            return;
        }

        returnCurlHandle(curl);
        this->containers = localcontainers;
    }
}

/**
 * @brief 수집된 컨테이너 정보 목록을 반환합니다
 *
 * @return 수집된 Docker 컨테이너 정보의 벡터
 */
vector<DockerContainerInfo> DockerCollector::getContainers() const
{
    return containers;
}