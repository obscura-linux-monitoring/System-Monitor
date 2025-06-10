#include "collectors/service_collector.h"
#include "models/service_info.h"
#include "log/logger.h"
#include <cstdio>
#include <memory>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <future>
#include <chrono>
#include <map>
#include <functional>
#include <systemd/sd-bus.h>

using namespace std;

/**
 * @brief 서비스 수집기 생성자
 *
 * @param threads 병렬 처리에 사용할 스레드 수
 * @param useNative 네이티브 API 사용 여부
 */
ServiceCollector::ServiceCollector(int threads, bool useNative) : maxThreads(max(1, threads)),
                                                                  useNativeAPI(useNative),
                                                                  stopBackgroundThread(false)
{
    // 백그라운드 업데이트 시작
    startBackgroundUpdate();
}

/**
 * @brief 네이티브 API 사용 여부 설정
 *
 * @param use 네이티브 API 사용 여부
 */
void ServiceCollector::setUseNativeAPI(bool use)
{
    useNativeAPI = use;
}

/**
 * @brief 서비스 정보 수집 메서드
 *
 * 백그라운드에서 이미 수집된 데이터를 사용하고 필요한 경우 특정 서비스에 대한 추가 정보만 업데이트합니다.
 */
void ServiceCollector::collect()
{
    // 백그라운드에서 이미 수집된 데이터 사용
    lock_guard<mutex> lock(serviceDataMutex);

    // 필요한 경우 특정 서비스에 대한 추가 정보만 업데이트
    for (auto &serviceId : requiredDetailedServices)
    {
        auto it = serviceInfoCache.find(serviceId);
        if (it != serviceInfoCache.end())
        {
            ServiceInfo tempInfo = it->second.info;
            collectServiceDetails(tempInfo, true);
            it->second.info = tempInfo;
        }
    }

    // 항상 최신 정보로 결과 벡터 업데이트
    serviceInfo.clear();
    for (const auto &pair : serviceInfoCache)
    {
        serviceInfo.push_back(pair.second.info);
    }
}

/**
 * @brief 모든 서비스 정보를 최적화된 방식으로 수집
 *
 * 시스템 명령어를 실행하여 서비스 목록을 가져오고, 변경된 서비스를 감지하여 업데이트합니다.
 * 병렬 처리와 캐싱을 통해 성능을 최적화합니다.
 */
void ServiceCollector::collectAllServicesInfo()
{
    LOG_INFO("전체 서비스 정보 수집 시작");

    // 메모리 재할당 줄이기
    currentServices.reserve(serviceInfoCache.size() + 10);

    // 결과 문자열 버퍼 미리 할당
    result.reserve(65536); // 64KB 미리 할당

    // 1. 명령어 실행 최소화: 모든 서비스 기본 정보를 한 번에 가져오기
    string cmd = "systemctl list-units --type=service --all --no-legend --no-pager "
                 "--output=json 2>/dev/null";

    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        LOG_ERROR("서비스 목록 명령 실행 실패");
        return;
    }

    // 결과 읽기
    string commandResult;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        commandResult += buffer;
    }
    pclose(pipe);

    // 현재 시스템의 서비스 목록 수집
    vector<ServiceInfo> tempServices;

    // JSON 파싱 (간단한 구현으로 대체 - 실제로는 JSON 라이브러리 사용 권장)
    // 여기서는 파싱 코드를 생략하고, 기존 방식으로 진행합니다
    collectServiceList(tempServices);

    // 서비스 목록 확인 로그 추가
    LOG_INFO("수집된 서비스 수: {}", tempServices.size());

    // MySQL 서비스 상태 로깅
    for (const auto &service : tempServices)
    {
        if (service.name == "mysql")
        {
            LOG_INFO("MySQL 서비스 현재 상태 - active_state: {}, status: {}",
                     service.active_state, service.status);
        }
    }

    // 2. 변경 감지 및 캐싱 메커니즘
    {
        lock_guard<mutex> lock(cacheMutex);

        // 새 서비스 추가 및 변경 서비스 표시
        for (auto &service : tempServices)
        {
            auto it = serviceInfoCache.find(service.name);
            if (it == serviceInfoCache.end())
            {
                // 새 서비스 - 캐시에 추가
                LOG_INFO("새 서비스 발견: {}", service.name);
                CachedServiceInfo cachedInfo;
                cachedInfo.info = service;
                cachedInfo.lastUpdated = chrono::system_clock::now();
                cachedInfo.needsUpdate = true;
                serviceInfoCache[service.name] = cachedInfo;
            }
            else if (hasServiceChanged(it->second.info, service))
            {
                // 변경된 서비스 감지 로그
                LOG_INFO("서비스 변경 감지: {} - 이전: {} -> 현재: {}",
                         service.name, it->second.info.active_state, service.active_state);

                // 변경된 서비스 - 업데이트 필요 표시
                it->second.info.status = service.status;
                it->second.info.active_state = service.active_state;
                it->second.info.enabled = service.enabled;
                it->second.needsUpdate = true;
            }
        }

        // MySQL 서비스 캐시 상태 확인
        auto mysqlIt = serviceInfoCache.find("mysql");
        if (mysqlIt != serviceInfoCache.end())
        {
            LOG_INFO("MySQL 서비스 캐시 상태 - active_state: {}, status: {}, needsUpdate: {}",
                     mysqlIt->second.info.active_state,
                     mysqlIt->second.info.status,
                     mysqlIt->second.needsUpdate ? "true" : "false");
        }
    }

    // 3. 병렬 처리 도입
    futures.clear();

    // 스레드 풀로 변경된 서비스만 업데이트
    {
        // 업데이트가 필요한 서비스 목록 먼저 수집
        vector<string> servicesToUpdate;
        {
            lock_guard<mutex> lock(cacheMutex);
            for (auto &pair : serviceInfoCache)
            {
                if (pair.second.needsUpdate)
                {
                    servicesToUpdate.push_back(pair.first);
                }
            }
        }

        // 락 해제 후 스레드 생성
        futures.clear();
        int runningThreads = 0;

        for (const auto &serviceName : servicesToUpdate)
        {
            // 최대 스레드 수 제한
            if (runningThreads >= maxThreads)
            {
                // 이전 작업이 완료될 때까지 대기
                for (auto &f : futures)
                {
                    if (f.valid())
                    {
                        f.wait();
                    }
                }
                futures.clear();
                runningThreads = 0;
            }

            ServiceInfo tempServiceInfo;
            {
                lock_guard<mutex> lock(cacheMutex);
                auto it = serviceInfoCache.find(serviceName);
                if (it != serviceInfoCache.end())
                {
                    tempServiceInfo = it->second.info;
                }
                else
                {
                    continue; // 서비스가 이미 삭제됨
                }
            }

            // 락 없이 작업 시작
            futures.push_back(async(launch::async,
                                    [this, serviceName, tempServiceInfo]() mutable
                                    {
                                        collectServiceDetails(tempServiceInfo);

                                        lock_guard<mutex> lock(cacheMutex);
                                        auto it = serviceInfoCache.find(serviceName);
                                        if (it != serviceInfoCache.end())
                                        {
                                            it->second.info = tempServiceInfo;
                                            it->second.lastUpdated = chrono::system_clock::now();
                                            it->second.needsUpdate = false;
                                        }
                                    }));

            runningThreads++;
        }
    }

    // 모든 작업이 완료될 때까지 대기
    for (auto &f : futures)
    {
        if (f.valid())
        {
            f.wait();
        }
    }

    // 캐시에서 결과 벡터로 복사
    {
        lock_guard<mutex> lock(cacheMutex);
        serviceInfo.clear();
        for (const auto &pair : serviceInfoCache)
        {
            serviceInfo.push_back(pair.second.info);
        }
    }
}

/**
 * @brief 두 서비스 정보 간의 중요한 변경 여부 확인
 *
 * @param oldInfo 이전 서비스 정보
 * @param newInfo 새 서비스 정보
 * @return bool 중요한 변경이 있으면 true, 없으면 false
 */
bool ServiceCollector::hasServiceChanged(const ServiceInfo &oldInfo, const ServiceInfo &newInfo) const
{
    bool changed = (oldInfo.status != newInfo.status ||
                    oldInfo.enabled != newInfo.enabled ||
                    oldInfo.active_state != newInfo.active_state);

    // MySQL 서비스의 변경 감지 확인
    if (oldInfo.name == "mysql" || newInfo.name == "mysql")
    {
        LOG_INFO("MySQL 서비스 변경 감지 비교 - 이전: {}, 현재: {}, 변경여부: {}",
                 oldInfo.active_state, newInfo.active_state,
                 changed ? "true" : "false");
    }

    return changed;
}

/**
 * @brief 기존 서비스 목록 수집 메서드
 *
 * systemctl 명령어를 사용하여 시스템의 모든 서비스 목록을 가져옵니다.
 *
 * @param result 수집된 서비스 정보를 저장할 벡터
 */
void ServiceCollector::collectServiceList(vector<ServiceInfo> &serviceList)
{
    // 모든 서비스를 한 번에 가져오는 명령으로 최적화
    FILE *pipe = popen("systemctl list-unit-files --type=service --no-pager --no-legend 2>/dev/null", "r");
    if (!pipe)
    {
        LOG_ERROR("서비스 목록 명령 실행 실패");
        return;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        string line(buffer);
        istringstream iss(line);
        string name, status;

        if (iss >> name >> status)
        {
            // .service 확장자 제거 - 더 효율적인 방법
            const string suffix = ".service";
            if (name.length() >= suffix.length() &&
                name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0)
            {
                name = name.substr(0, name.length() - suffix.length());
            }

            ServiceInfo info;
            info.name = name;
            info.enabled = (status == "enabled");
            info.status = status;

            serviceList.push_back(info);
        }
    }

    int status = pclose(pipe);
    if (status == -1)
    {
        LOG_ERROR("서비스 목록 명령 종료 실패");
    }
}

/**
 * @brief 서비스 상세 정보 수집
 *
 * 특정 서비스의 상세 정보(타입, 상태, 자원 사용량 등)를 수집합니다.
 * 캐싱 메커니즘을 사용하여 불필요한 업데이트를 방지합니다.
 *
 * @param service 상세 정보를 수집할 서비스 객체
 * @param forceUpdate 강제 업데이트 여부, 기본값은 false
 */
void ServiceCollector::collectServiceDetails(ServiceInfo &service, bool forceUpdate)
{
    // 캐싱된 정보 확인 - 강제 업데이트가 아니면 캐시 사용
    if (!forceUpdate)
    {
        lock_guard<mutex> lock(cacheMutex);
        auto it = serviceInfoCache.find(service.name);
        if (it != serviceInfoCache.end() && !it->second.needsUpdate)
        {
            // 캐시된 정보가 있고 최신 상태면 사용
            auto now = chrono::system_clock::now();
            auto elapsed = chrono::duration_cast<chrono::seconds>(
                               now - it->second.lastUpdated)
                               .count();

            // 30초 이내 업데이트된 정보는 재사용
            if (elapsed < 30)
            {
                service = it->second.info;
                return;
            }
        }
    }

    // 모든 속성을 한 번에 가져오도록 최적화
    string cmd = "systemctl show " + service.name +
                 ".service -p Type,LoadState,ActiveState,SubState,MainPID 2>/dev/null";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        LOG_ERROR("서비스 상세 정보 명령 실행 실패: {}", service.name);
        return;
    }

    string mainPID;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        string line(buffer);
        size_t pos = line.find('=');
        if (pos != string::npos)
        {
            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);

            // 줄 끝의 개행 문자 제거
            if (!value.empty() && value.back() == '\n')
            {
                value.pop_back();
            }

            if (key == "Type")
                service.type = value;
            else if (key == "LoadState")
                service.load_state = value;
            else if (key == "ActiveState")
                service.active_state = value;
            else if (key == "SubState")
                service.sub_state = value;
            else if (key == "MainPID")
                mainPID = value;
        }
    }

    int status = pclose(pipe);
    if (status == -1)
    {
        LOG_ERROR("서비스 상세 정보 명령 종료 실패: {}", service.name);
    }

    // MainPID를 이미 가져왔으므로 추가 호출 필요 없음
    if (!mainPID.empty() && mainPID != "0")
    {
        collectResourceUsageByPID(service, mainPID);
    }
    else
    {
        // 실행 중이 아니면 자원 사용량은 0으로 설정
        service.memory_usage = 0;
        service.cpu_usage = 0.0f;
    }

    // 캐시 업데이트
    updateCachedInfo(service.name, service);
}

/**
 * @brief 서비스의 자원 사용량 수집
 *
 * 프로세스 ID를 기반으로 서비스의 메모리 사용량과 CPU 사용률을 수집합니다.
 *
 * @param service 자원 사용량을 수집할 서비스 객체
 * @param pid 서비스의 프로세스 ID
 */
void ServiceCollector::collectResourceUsageByPID(ServiceInfo &service, const string &pid)
{
    try
    {
        // 한 번의 ps 명령으로 CPU와 메모리 정보 모두 가져오기
        string cmd = "ps -o rss=,pcpu= -p " + pid + " 2>/dev/null";
        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe)
        {
            LOG_ERROR("자원 사용량 명령 실행 실패: {}", service.name);
            return;
        }

        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            string line(buffer);
            istringstream iss(line);
            string rss_str, cpu_str;

            if (iss >> rss_str >> cpu_str)
            {
                try
                {
                    service.memory_usage = stoull(rss_str) * 1024; // KB를 바이트로 변환
                    service.cpu_usage = stof(cpu_str);
                }
                catch (const exception &e)
                {
                    LOG_ERROR("자원 사용량 변환 오류: {}", e.what());
                    service.memory_usage = 0;
                    service.cpu_usage = 0.0f;
                }
            }
        }

        int status = pclose(pipe);
        if (status == -1)
        {
            LOG_ERROR("자원 사용량 명령 종료 실패: {}", service.name);
        }
    }
    catch (const exception &e)
    {
        LOG_ERROR("자원 사용량 수집 중 오류: {}", e.what());
        service.memory_usage = 0;
        service.cpu_usage = 0.0f;
    }
}

/**
 * @brief 캐시 정보 업데이트
 *
 * 서비스 정보를 캐시에 저장하고 최종 업데이트 시간을 기록합니다.
 *
 * @param name 서비스 이름
 * @param info 업데이트할 서비스 정보
 */
void ServiceCollector::updateCachedInfo(const string &name, const ServiceInfo &info)
{
    lock_guard<mutex> lock(cacheMutex);
    auto it = serviceInfoCache.find(name);
    if (it != serviceInfoCache.end())
    {
        it->second.info = info;
        it->second.lastUpdated = chrono::system_clock::now();
        it->second.needsUpdate = false;
    }
    else
    {
        CachedServiceInfo cachedInfo;
        cachedInfo.info = info;
        cachedInfo.lastUpdated = chrono::system_clock::now();
        cachedInfo.needsUpdate = false;
        serviceInfoCache[name] = cachedInfo;
    }
}

/**
 * @brief 더 이상 존재하지 않는 서비스 제거
 *
 * 현재 시스템에 존재하지 않는 서비스를 결과 목록에서 제거합니다.
 *
 * @param currentServices 현재 시스템에 존재하는 서비스 목록
 */
void ServiceCollector::removeObsoleteServices(const vector<ServiceInfo> &activeServices)
{
    // 현재 시스템에 존재하지 않는 서비스 제거
    serviceInfo.erase(
        remove_if(serviceInfo.begin(), serviceInfo.end(),
                  [&activeServices](const ServiceInfo &info)
                  {
                      return find_if(activeServices.begin(), activeServices.end(),
                                     [&info](const ServiceInfo &current)
                                     {
                                         return current.name == info.name;
                                     }) == activeServices.end();
                  }),
        serviceInfo.end());
}

/**
 * @brief 모든 서비스 정보 초기화
 *
 * 수집된 서비스 정보와 캐시를 모두 비웁니다.
 */
void ServiceCollector::clear()
{
    serviceInfo.clear();
    lock_guard<mutex> lock(cacheMutex);
    serviceInfoCache.clear();
}

/**
 * @brief 수집된 서비스 정보 반환 (복사본)
 *
 * @return vector<ServiceInfo> 서비스 정보 벡터의 복사본
 */
vector<ServiceInfo> ServiceCollector::getServiceInfo() const
{
    return serviceInfo;
}

/**
 * @brief 수집된 서비스 정보 참조 반환
 *
 * @return const vector<ServiceInfo>& 서비스 정보 벡터의 참조
 */
const vector<ServiceInfo> &ServiceCollector::getServiceInfoRef() const
{
    return serviceInfo;
}

/**
 * @brief 특정 서비스의 정보 업데이트
 *
 * 이름으로 서비스를 찾아 정보를 업데이트하거나, 없는 경우 새로 추가합니다.
 *
 * @param newInfo 업데이트할 서비스 정보
 */
void ServiceCollector::updateServiceInfo(const ServiceInfo &newInfo)
{
    // 이름으로 서비스 찾기
    auto it = find_if(serviceInfo.begin(), serviceInfo.end(),
                      [&newInfo](const ServiceInfo &info)
                      { return info.name == newInfo.name; });

    if (it != serviceInfo.end())
    {
        // 기존 서비스 정보 업데이트
        *it = newInfo;
    }
    else
    {
        // 새 서비스 추가
        serviceInfo.push_back(newInfo);
    }

    // 캐시 업데이트
    updateCachedInfo(newInfo.name, newInfo);
}

/**
 * @brief 네이티브 API를 사용한 정보 수집
 *
 * systemd의 네이티브 D-Bus API를 사용하여 서비스 정보를 수집합니다.
 */
void ServiceCollector::collectUsingNativeAPI()
{
    LOG_INFO("네이티브 systemd API를 사용한 서비스 정보 수집");
    int r;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;

    // 연결 재사용 - 전역 또는 클래스 멤버로 sd_bus 객체 유지
    if (!m_bus)
    {
        r = sd_bus_open_system(&m_bus);
        if (r < 0)
        {
            LOG_ERROR("시스템 버스 연결 실패: {}", strerror(-r));
            return;
        }
    }

    // 연결 재사용
    sd_bus *bus = m_bus;

    // 현재 서비스 목록을 저장할 벡터
    vector<ServiceInfo> discoveredServices;

    // 모든 systemd 유닛 목록 요청
    r = sd_bus_call_method(bus,
                           "org.freedesktop.systemd1",         // 서비스 이름
                           "/org/freedesktop/systemd1",        // 객체 경로
                           "org.freedesktop.systemd1.Manager", // 인터페이스
                           "ListUnits",                        // 메소드
                           &error,                             // 오류
                           &reply,                             // 응답
                           "");                                // 인자 (없음)

    if (r < 0)
    {
        LOG_ERROR("ListUnits 메소드 호출 실패: {}", error.message);
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        return;
    }

    // 응답 파싱 시작
    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (r < 0)
    {
        LOG_ERROR("메시지 컨테이너 진입 실패: {}", strerror(-r));
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        return;
    }

    // 서비스 리스트 루프
    while ((r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_STRUCT, "ssssssouso")) > 0)
    {
        const char *name, *description, *load_state, *active_state, *sub_state, *unit_path;
        uint32_t job_id;
        const char *job_type, *job_path;

        // 구조체 필드 읽기
        r = sd_bus_message_read(reply, "ssssssouso",
                                &name, &description, &load_state,
                                &active_state, &sub_state, &unit_path,
                                &job_id, &job_type, &job_path);

        if (r < 0)
        {
            LOG_ERROR("서비스 정보 읽기 실패: {}", strerror(-r));
            continue;
        }

        // 서비스 유형만 필터링
        if (strstr(name, ".service") != NULL)
        {
            ServiceInfo info;

            // .service 확장자 제거
            string service_name = name;
            const string suffix = ".service";
            if (service_name.length() >= suffix.length() &&
                service_name.compare(service_name.length() - suffix.length(), suffix.length(), suffix) == 0)
            {
                service_name = service_name.substr(0, service_name.length() - suffix.length());
            }

            info.name = service_name;
            info.load_state = load_state;
            info.active_state = active_state;
            info.sub_state = sub_state;

            // 추가 서비스 세부 정보 수집
            collectServiceDetailsNative(info);

            discoveredServices.push_back(info);
        }

        sd_bus_message_exit_container(reply);
    }

    sd_bus_message_unref(reply);

    // 서비스 활성화 상태 가져오기
    map<string, bool> enabledStates;
    collectAllEnabledStatesAtOnce(bus, enabledStates);

    for (auto &service : discoveredServices)
    {
        auto it = enabledStates.find(service.name);
        if (it != enabledStates.end())
        {
            service.enabled = it->second;
            service.status = it->second ? "enabled" : "disabled";
        }
    }

    sd_bus_unref(bus);

    // 기존 서비스 정보 업데이트
    {
        lock_guard<mutex> lock(cacheMutex);
        serviceInfoCache.clear();

        for (const auto &service : discoveredServices)
        {
            CachedServiceInfo cachedInfo;
            cachedInfo.info = service;
            cachedInfo.lastUpdated = chrono::system_clock::now();
            cachedInfo.needsUpdate = false;
            serviceInfoCache[service.name] = cachedInfo;
        }

        // 결과 벡터 업데이트
        serviceInfo.clear();
        for (const auto &pair : serviceInfoCache)
        {
            serviceInfo.push_back(pair.second.info);
        }
    }
}

/**
 * @brief 네이티브 API를 사용한 서비스 상세 정보 수집
 *
 * systemd의 D-Bus API를 사용하여 특정 서비스의 상세 정보를 수집합니다.
 *
 * @param service 상세 정보를 수집할 서비스 객체
 */
void ServiceCollector::collectServiceDetailsNative(ServiceInfo &service)
{
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_open_system(&bus);
    if (r < 0)
    {
        LOG_ERROR("서비스 상세 정보 수집 시 시스템 버스 연결 실패: {}", strerror(-r));
        return;
    }

    // 서비스 타입 가져오기
    const char *service_path = NULL;
    r = sd_bus_call_method(bus,
                           "org.freedesktop.systemd1",
                           "/org/freedesktop/systemd1",
                           "org.freedesktop.systemd1.Manager",
                           "LoadUnit",
                           &error,
                           &reply,
                           "s", (service.name + ".service").c_str());

    if (r >= 0)
    {
        r = sd_bus_message_read(reply, "o", &service_path);
        sd_bus_message_unref(reply);

        if (r >= 0 && service_path != NULL)
        {
            // 여러 속성을 단일 호출로 가져오기
            if (GetMultipleProperties(bus, service_path, service))
            {
                // 서비스 정보가 이미 설정됨
                LOG_INFO("서비스 상세 정보 수집 완료: {}", service.name);
            }
            else
            {
                LOG_ERROR("서비스 상세 정보 수집 실패: {}", service.name);
            }
        }
    }
    else
    {
        LOG_ERROR("서비스 로드 실패: {}", error.message);
        sd_bus_error_free(&error);
    }

    sd_bus_unref(bus);

    // 캐시 업데이트
    updateCachedInfo(service.name, service);
}

/**
 * @brief 모든 서비스의 활성화 상태를 한 번에 수집
 *
 * D-Bus API를 사용하여 모든 서비스의 활성화 상태를 효율적으로 수집합니다.
 *
 * @param bus D-Bus 연결 객체
 * @param enabledStates 서비스 이름과 활성화 상태를 매핑할 맵
 */
void ServiceCollector::collectAllEnabledStatesAtOnce(sd_bus *bus, map<string, bool> &enabledStates)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_call_method(bus,
                           "org.freedesktop.systemd1",
                           "/org/freedesktop/systemd1",
                           "org.freedesktop.systemd1.Manager",
                           "ListUnitFiles",
                           &error,
                           &reply,
                           "");

    if (r < 0)
    {
        LOG_ERROR("ListUnitFiles 메소드 호출 실패: {}", error.message);
        sd_bus_error_free(&error);
        return;
    }

    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ss)");
    if (r < 0)
    {
        LOG_ERROR("유닛 파일 배열 컨테이너 진입 실패: {}", strerror(-r));
        sd_bus_message_unref(reply);
        return;
    }

    // 모든 유닛 파일 반복
    while ((r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_STRUCT, "ss")) > 0)
    {
        const char *unit_path, *state;

        r = sd_bus_message_read(reply, "ss", &unit_path, &state);
        if (r < 0)
        {
            continue;
        }

        // 서비스 유닛만 처리
        string unitPath(unit_path);
        if (unitPath.find(".service") != string::npos)
        {
            // 유닛 이름 추출
            size_t lastSlash = unitPath.find_last_of('/');
            size_t servicePos = unitPath.find(".service");

            if (lastSlash == string::npos)
            {
                lastSlash = 0;
            }
            else
            {
                lastSlash++;
            }

            if (servicePos != string::npos)
            {
                string serviceName = unitPath.substr(lastSlash, servicePos - lastSlash);
                enabledStates[serviceName] = (string(state) == "enabled");
            }
        }

        sd_bus_message_exit_container(reply);
    }

    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
}

/**
 * @brief 기본 서비스 정보만 수집
 *
 * 서비스의 기본 정보만 빠르게 수집합니다. 전체 업데이트와 부분 업데이트를 시간에 따라 결정합니다.
 */
void ServiceCollector::collectBasicServiceInfo()
{
    auto now = chrono::system_clock::now();
    static auto lastCompleteUpdate = now;

    // 5초 이내에는 변경된 서비스만 업데이트
    bool partialUpdate = chrono::duration_cast<chrono::seconds>(
                             now - lastCompleteUpdate)
                             .count() < 5;

    if (partialUpdate)
    {
        // 변경된 서비스만 빠르게 업데이트
        updateChangedServicesOnly();
    }
    else
    {
        // 전체 서비스 정보 업데이트
        if (useNativeAPI)
        {
            collectUsingNativeAPI();
        }
        else
        {
            collectAllServicesInfo();
        }
        lastCompleteUpdate = now;
    }
}

/**
 * @brief 변경된 서비스만 업데이트
 *
 * 시스템에서 변경된 서비스만 식별하여 업데이트합니다.
 */
void ServiceCollector::updateChangedServicesOnly()
{
    // 모든 서비스의 현재 상태를 확인하는 명령어로 변경
    FILE *pipe = popen("systemctl list-units --type=service --all --no-legend --no-pager 2>/dev/null", "r");
    if (!pipe)
    {
        LOG_ERROR("서비스 목록 명령 실행 실패");
        return;
    }

    map<string, string> currentActiveStates;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        string line(buffer);
        istringstream iss(line);
        string name, load, active, sub, description;

        if (iss >> name >> load >> active >> sub)
        {
            // .service 확장자 제거
            const string suffix = ".service";
            if (name.length() >= suffix.length() &&
                name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0)
            {
                name = name.substr(0, name.length() - suffix.length());
            }
            currentActiveStates[name] = active; // active 상태 저장
        }
    }
    pclose(pipe);

    // 캐시와 현재 상태 비교하여 변경된 서비스 찾기
    vector<string> changedServices;
    {
        lock_guard<mutex> lock(cacheMutex);
        for (auto &pair : serviceInfoCache)
        {
            auto it = currentActiveStates.find(pair.first);
            if (it != currentActiveStates.end())
            {
                // 디버깅을 위한 로그 추가
                LOG_INFO("서비스 상태 비교: {} - 캐시: '{}' vs 현재: '{}'",
                         pair.first, pair.second.info.active_state, it->second);

                // 캐시된 상태와 현재 상태 비교
                if (pair.second.info.active_state != it->second)
                {
                    LOG_INFO("서비스 상태 변경 감지: {} - '{}' -> '{}'",
                             pair.first, pair.second.info.active_state, it->second);

                    changedServices.push_back(pair.first);
                    // 즉시 상태 업데이트 (더 빠른 반응을 위해)
                    pair.second.info.active_state = it->second;
                    pair.second.needsUpdate = true;
                }
            }
        }
    }

    // 변경된 서비스의 상세 정보 업데이트
    for (const auto &name : changedServices)
    {
        ServiceInfo tempInfo;
        {
            lock_guard<mutex> lock(cacheMutex);
            auto it = serviceInfoCache.find(name);
            if (it != serviceInfoCache.end())
            {
                tempInfo = it->second.info;
            }
            else
            {
                continue;
            }
        }

        collectServiceDetails(tempInfo, true);
        updateCachedInfo(name, tempInfo);
    }

    // 캐시에서 결과 벡터로 복사
    {
        lock_guard<mutex> lock(cacheMutex);
        serviceInfo.clear();
        for (const auto &pair : serviceInfoCache)
        {
            serviceInfo.push_back(pair.second.info);
        }
    }
}

/**
 * @brief 여러 속성을 한 번에 가져오는 도우미 메서드
 *
 * D-Bus API를 사용하여 여러 속성을 단일 호출로 효율적으로 가져옵니다.
 *
 * @param bus D-Bus 연결 객체
 * @param service_path 서비스 경로
 * @param properties 가져올 속성 배열
 * @param count 속성 배열의 크기
 * @param service 서비스 객체
 * @return bool 속성 가져오기 성공 여부
 */
bool ServiceCollector::GetMultipleProperties(sd_bus *bus, const char *service_path,
                                             ServiceInfo &service)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;

    // 단일 호출로 여러 속성 요청
    r = sd_bus_call_method(bus,
                           "org.freedesktop.systemd1",
                           service_path,
                           "org.freedesktop.DBus.Properties",
                           "GetAll",
                           &error,
                           &reply,
                           "s", "org.freedesktop.systemd1.Service");

    if (r < 0)
    {
        LOG_ERROR("속성 가져오기 실패: {}", error.message);
        sd_bus_error_free(&error);
        return false;
    }

    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "{sv}");
    if (r < 0)
    {
        LOG_ERROR("속성 컨테이너 진입 실패: {}", strerror(-r));
        sd_bus_message_unref(reply);
        return false;
    }

    // 모든 속성 순회
    while (sd_bus_message_enter_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv") > 0)
    {
        const char *prop_name;
        r = sd_bus_message_read_basic(reply, SD_BUS_TYPE_STRING, &prop_name);

        if (r >= 0)
        {
            r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_VARIANT, NULL);
            if (r >= 0)
            {
                // Type 속성 처리
                if (strcmp(prop_name, "Type") == 0)
                {
                    const char *value;
                    r = sd_bus_message_read_basic(reply, SD_BUS_TYPE_STRING, &value);
                    if (r >= 0)
                    {
                        service.type = value;
                    }
                }
                // MainPID 속성 처리
                else if (strcmp(prop_name, "MainPID") == 0)
                {
                    uint32_t pid;
                    r = sd_bus_message_read_basic(reply, SD_BUS_TYPE_UINT32, &pid);
                    if (r >= 0 && pid > 0)
                    {
                        // PID를 이용해 자원 사용량 수집 가능
                        string pidStr = to_string(pid);
                        collectResourceUsageByPID(service, pidStr);
                    }
                }
                // MemoryCurrent 속성 처리
                else if (strcmp(prop_name, "MemoryCurrent") == 0)
                {
                    uint64_t memory;
                    r = sd_bus_message_read_basic(reply, SD_BUS_TYPE_UINT64, &memory);
                    if (r >= 0)
                    {
                        service.memory_usage = memory;
                    }
                }
                // CPUUsageNSec 속성 처리
                else if (strcmp(prop_name, "CPUUsageNSec") == 0)
                {
                    uint64_t cpu_nsec;
                    r = sd_bus_message_read_basic(reply, SD_BUS_TYPE_UINT64, &cpu_nsec);
                    if (r >= 0)
                    {
                        // 나노초 단위 CPU 사용시간을 백분율로 변환
                        // 실제 변환은 시스템에 따라 조정 필요
                        static uint64_t last_time = 0;
                        static uint64_t last_cpu_nsec = 0;

                        uint64_t current_time = static_cast<uint64_t>(chrono::duration_cast<chrono::nanoseconds>(
                                                                          chrono::system_clock::now().time_since_epoch())
                                                                          .count());

                        if (last_time > 0 && last_cpu_nsec > 0)
                        {
                            uint64_t time_diff = current_time - last_time;
                            uint64_t cpu_diff = cpu_nsec - last_cpu_nsec;

                            if (time_diff > 0)
                            {
                                // CPU 사용률을 백분율로 계산
                                service.cpu_usage = static_cast<float>((static_cast<double>(cpu_diff) / static_cast<double>(time_diff)) * 100.0);
                            }
                        }

                        last_time = current_time;
                        last_cpu_nsec = cpu_nsec;
                    }
                }

                sd_bus_message_exit_container(reply); // variant 컨테이너 종료
            }
        }

        sd_bus_message_exit_container(reply); // dict_entry 컨테이너 종료
    }

    sd_bus_message_exit_container(reply); // array 컨테이너 종료
    sd_bus_message_unref(reply);

    return true;
}

/**
 * @brief 소멸자
 *
 * 백그라운드 업데이트를 중지하고 D-Bus 연결을 해제합니다.
 */
ServiceCollector::~ServiceCollector()
{
    // 백그라운드 업데이트 중지
    stopBackgroundUpdate();

    if (m_bus)
    {
        sd_bus_unref(m_bus);
        m_bus = nullptr;
    }
}

/**
 * @brief 상세 정보 수집이 필요한 서비스 추가
 *
 * 우선적으로 상세 정보를 수집할 서비스 목록에 추가합니다.
 *
 * @param serviceName 추가할 서비스 이름
 */
void ServiceCollector::addRequiredDetailedService(const string &serviceName)
{
    if (find(requiredDetailedServices.begin(), requiredDetailedServices.end(), serviceName) == requiredDetailedServices.end())
    {
        requiredDetailedServices.push_back(serviceName);
    }
}

/**
 * @brief 상세 정보 수집 목록 초기화
 *
 * 우선적으로 상세 정보를 수집할 서비스 목록을 비웁니다.
 */
void ServiceCollector::clearRequiredDetailedServices()
{
    requiredDetailedServices.clear();
}

/**
 * @brief 백그라운드 태스크 실행
 *
 * 백그라운드에서 주기적으로 서비스 정보를 수집하는 태스크입니다.
 */
void ServiceCollector::backgroundUpdateTask()
{
    while (!stopBackgroundThread)
    {
        {
            // 전체 서비스 정보 업데이트 - 메인 스레드와 별도로 실행
            vector<ServiceInfo> collectedServices;

            if (useNativeAPI)
            {
                collectedServices = collectServicesNativeAsync();
            }
            else
            {
                collectedServices = collectAllServicesInfoAsync();
            }

            // 결과를 메인 데이터 구조에 안전하게 병합
            lock_guard<mutex> lock(serviceDataMutex);
            mergeServiceData(collectedServices);
        }

        // 5초 간격으로 업데이트
        for (int i = 0; i < 50 && !stopBackgroundThread; i++)
        {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
}

/**
 * @brief 백그라운드 업데이트 시작
 *
 * 백그라운드 스레드를 생성하여 주기적인 정보 수집을 시작합니다.
 */
void ServiceCollector::startBackgroundUpdate()
{
    stopBackgroundThread = false;
    backgroundUpdateThread = thread(&ServiceCollector::backgroundUpdateTask, this);
}

/**
 * @brief 백그라운드 업데이트 중지
 *
 * 백그라운드 스레드를 안전하게 종료합니다.
 */
void ServiceCollector::stopBackgroundUpdate()
{
    stopBackgroundThread = true;
    if (backgroundUpdateThread.joinable())
    {
        backgroundUpdateThread.join();
    }
}

/**
 * @brief 비동기 네이티브 API 사용 서비스 수집
 *
 * 백그라운드 스레드에서 네이티브 API를 사용하여 서비스 정보를 수집합니다.
 *
 * @return vector<ServiceInfo> 수집된 서비스 정보 벡터
 */
vector<ServiceInfo> ServiceCollector::collectServicesNativeAsync()
{
    vector<ServiceInfo> infoResult;

    LOG_INFO("비동기 네이티브 systemd API 서비스 정보 수집");
    int r;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    sd_bus *bus = NULL;

    // 로컬 버스 연결 생성 (스레드 안전성 위해)
    r = sd_bus_open_system(&bus);
    if (r < 0)
    {
        LOG_ERROR("비동기 시스템 버스 연결 실패: {}", strerror(-r));
        return infoResult;
    }

    // 모든 systemd 유닛 목록 요청
    r = sd_bus_call_method(bus,
                           "org.freedesktop.systemd1",
                           "/org/freedesktop/systemd1",
                           "org.freedesktop.systemd1.Manager",
                           "ListUnits",
                           &error,
                           &reply,
                           "");

    if (r < 0)
    {
        LOG_ERROR("비동기 ListUnits 메소드 호출 실패: {}", error.message);
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        return infoResult;
    }

    // 응답 파싱 시작
    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (r < 0)
    {
        LOG_ERROR("비동기 메시지 컨테이너 진입 실패: {}", strerror(-r));
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        return infoResult;
    }

    // 서비스 리스트 루프
    while ((r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_STRUCT, "ssssssouso")) > 0)
    {
        const char *name, *description, *load_state, *active_state, *sub_state, *unit_path;
        uint32_t job_id;
        const char *job_type, *job_path;

        // 구조체 필드 읽기
        r = sd_bus_message_read(reply, "ssssssouso",
                                &name, &description, &load_state,
                                &active_state, &sub_state, &unit_path,
                                &job_id, &job_type, &job_path);

        if (r < 0)
        {
            LOG_ERROR("비동기 서비스 정보 읽기 실패: {}", strerror(-r));
            continue;
        }

        // 서비스 유형만 필터링
        if (strstr(name, ".service") != NULL)
        {
            ServiceInfo info;

            // .service 확장자 제거
            string service_name = name;
            const string suffix = ".service";
            if (service_name.length() >= suffix.length() &&
                service_name.compare(service_name.length() - suffix.length(), suffix.length(), suffix) == 0)
            {
                service_name = service_name.substr(0, service_name.length() - suffix.length());
            }

            info.name = service_name;
            info.load_state = load_state;
            info.active_state = active_state;
            info.sub_state = sub_state;

            // 스레드 안전을 위해 별도 함수 호출 대신 간단한 상세 정보만 수집
            const char *service_path = NULL;
            sd_bus_message *detail_reply = NULL;

            // 서비스 경로 로드
            r = sd_bus_call_method(bus,
                                   "org.freedesktop.systemd1",
                                   "/org/freedesktop/systemd1",
                                   "org.freedesktop.systemd1.Manager",
                                   "LoadUnit",
                                   &error,
                                   &detail_reply,
                                   "s", (info.name + ".service").c_str());

            if (r >= 0)
            {
                r = sd_bus_message_read(detail_reply, "o", &service_path);
                sd_bus_message_unref(detail_reply);

                if (r >= 0 && service_path != NULL)
                {
                    // 중요 속성만 직접 가져오기
                    char *type_ptr = NULL;
                    r = sd_bus_get_property_string(bus,
                                                   "org.freedesktop.systemd1",
                                                   service_path,
                                                   "org.freedesktop.systemd1.Unit",
                                                   "Type",
                                                   &error,
                                                   &type_ptr);
                    if (r >= 0 && type_ptr)
                    {
                        info.type = type_ptr;
                        free(type_ptr);
                    }

                    // MainPID 정보 가져오기
                    uint32_t main_pid = 0;
                    r = sd_bus_get_property_trivial(bus,
                                                    "org.freedesktop.systemd1",
                                                    service_path,
                                                    "org.freedesktop.systemd1.Service",
                                                    "MainPID",
                                                    &error,
                                                    'u',
                                                    &main_pid);

                    // 활성 상태에 따라 자원 사용량 0으로 초기화
                    info.memory_usage = 0;
                    info.cpu_usage = 0.0f;
                }
            }

            // 에러 초기화
            sd_bus_error_free(&error);
            error = SD_BUS_ERROR_NULL;

            infoResult.push_back(info);
        }

        sd_bus_message_exit_container(reply);
    }

    sd_bus_message_unref(reply);

    // 서비스 활성화 상태 가져오기
    map<string, bool> enabledStates;

    // UnitFiles 목록 가져오기
    r = sd_bus_call_method(bus,
                           "org.freedesktop.systemd1",
                           "/org/freedesktop/systemd1",
                           "org.freedesktop.systemd1.Manager",
                           "ListUnitFiles",
                           &error,
                           &reply,
                           "");

    if (r >= 0)
    {
        r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ss)");
        if (r >= 0)
        {
            // 모든 유닛 파일 반복
            while ((r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_STRUCT, "ss")) > 0)
            {
                const char *unit_path, *state;

                r = sd_bus_message_read(reply, "ss", &unit_path, &state);
                if (r < 0)
                {
                    continue;
                }

                // 서비스 유닛만 처리
                string unitPath(unit_path);
                if (unitPath.find(".service") != string::npos)
                {
                    // 유닛 이름 추출
                    size_t lastSlash = unitPath.find_last_of('/');
                    size_t servicePos = unitPath.find(".service");

                    if (lastSlash == string::npos)
                    {
                        lastSlash = 0;
                    }
                    else
                    {
                        lastSlash++;
                    }

                    if (servicePos != string::npos)
                    {
                        string serviceName = unitPath.substr(lastSlash, servicePos - lastSlash);
                        enabledStates[serviceName] = (string(state) == "enabled");
                    }
                }

                sd_bus_message_exit_container(reply);
            }
        }
        sd_bus_message_unref(reply);
    }

    // 활성화 상태 업데이트
    for (auto &service : infoResult)
    {
        auto it = enabledStates.find(service.name);
        if (it != enabledStates.end())
        {
            service.enabled = it->second;
            service.status = it->second ? "enabled" : "disabled";
        }
    }

    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    return infoResult;
}

/**
 * @brief 비동기 서비스 정보 수집
 *
 * 백그라운드 스레드에서 효율적으로 서비스 정보를 수집합니다.
 *
 * @return vector<ServiceInfo> 수집된 서비스 정보 벡터
 */
vector<ServiceInfo> ServiceCollector::collectAllServicesInfoAsync()
{
    LOG_INFO("비동기 서비스 정보 수집 시작");
    vector<ServiceInfo> infoResult;

    // 모든 서비스 기본 정보를 한 번에 가져오기
    string cmd = "systemctl list-unit-files --type=service --no-pager --no-legend 2>/dev/null";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        LOG_ERROR("비동기 서비스 목록 명령 실행 실패");
        return infoResult;
    }

    // 서비스 기본 정보 수집
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        string line(buffer);
        istringstream iss(line);
        string name, status;

        if (iss >> name >> status)
        {
            // .service 확장자 제거
            const string suffix = ".service";
            if (name.length() >= suffix.length() &&
                name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0)
            {
                name = name.substr(0, name.length() - suffix.length());
            }

            ServiceInfo info;
            info.name = name;
            info.enabled = (status == "enabled");
            info.status = status;

            infoResult.push_back(info);

            // MySQL 서비스 로깅
            if (name == "mysql")
            {
                LOG_INFO("MySQL 서비스 기본 정보 수집: enabled={}, status={}",
                         info.enabled ? "true" : "false", info.status);
            }
        }
    }
    pclose(pipe);

    LOG_INFO("기본 서비스 정보 수집 완료: {} 개", infoResult.size());

    // 서비스별 상세 정보 수집 - 이 부분이 active_state를 가져옴
    for (auto &service : infoResult)
    {
        // 템플릿 서비스인지 확인 (@가 포함되어 있는지)
        bool isTemplateService = (service.name.find('@') != string::npos);

        string detailCmd;
        if (isTemplateService)
        {
            // 템플릿 서비스는 인스턴스 목록을 먼저 확인
            string instanceCmd = "systemctl list-units " + service.name + "*.service --no-legend --no-pager 2>/dev/null";
            FILE *instancePipe = popen(instanceCmd.c_str(), "r");
            if (instancePipe)
            {
                char instanceBuffer[512];
                // 첫 번째 인스턴스만 가져와서 처리 (있는 경우)
                if (fgets(instanceBuffer, sizeof(instanceBuffer), instancePipe) != nullptr)
                {
                    string instanceLine(instanceBuffer);
                    istringstream instanceIss(instanceLine);
                    string fullInstanceName;
                    instanceIss >> fullInstanceName; // 첫 번째 필드가 전체 인스턴스 이름

                    // 전체 인스턴스 이름이 있으면 이를 사용
                    if (!fullInstanceName.empty())
                    {
                        // .service 확장자는 이미 포함되어 있으므로 제거하지 않음
                        detailCmd = "systemctl show " + fullInstanceName +
                                    " -p Type,LoadState,ActiveState,SubState 2>/dev/null";
                    }
                    else
                    {
                        // 인스턴스가 없으면 원래 템플릿에 대해 정보 수집 시도
                        detailCmd = "systemctl show " + service.name +
                                    ".service -p Type,LoadState,ActiveState,SubState 2>/dev/null";
                    }
                }
                else
                {
                    // 인스턴스가 없는 경우 - 기본 정보만 유지하고 계속 진행
                    pclose(instancePipe);
                    continue;
                }
                pclose(instancePipe);
            }
            else
            {
                // 인스턴스 목록을 가져올 수 없는 경우 - 기본 정보만 유지하고 계속 진행
                continue;
            }
        }
        else
        {
            // 일반 서비스는 기존 방식으로 처리
            detailCmd = "systemctl show " + service.name +
                        ".service -p Type,LoadState,ActiveState,SubState 2>/dev/null";
        }

        // MySQL 서비스 상세 정보 명령 로깅
        if (service.name == "mysql")
        {
            LOG_INFO("MySQL 서비스 상세 정보 명령: {}", detailCmd);
        }

        // 상세 정보 수집
        FILE *detailPipe = popen(detailCmd.c_str(), "r");
        if (detailPipe)
        {
            while (fgets(buffer, sizeof(buffer), detailPipe) != nullptr)
            {
                string line(buffer);
                size_t pos = line.find('=');
                if (pos != string::npos)
                {
                    string key = line.substr(0, pos);
                    string value = line.substr(pos + 1);

                    // 줄 끝의 개행 문자 제거
                    if (!value.empty() && value.back() == '\n')
                    {
                        value.pop_back();
                    }

                    if (key == "Type")
                        service.type = value;
                    else if (key == "LoadState")
                        service.load_state = value;
                    else if (key == "ActiveState")
                        service.active_state = value;
                    else if (key == "SubState")
                        service.sub_state = value;

                    // MySQL 서비스 상세 정보 로깅
                    if (service.name == "mysql" && (key == "ActiveState" || key == "SubState"))
                    {
                        LOG_INFO("MySQL 서비스 상세 정보: {}={}", key, value);
                    }
                }
            }

            int status = pclose(detailPipe);
            if (status != 0)
            {
                LOG_ERROR("명령 실행 실패: {} (상태: {})", detailCmd, status);
            }
        }

        // MySQL 서비스 최종 상태 로깅
        if (service.name == "mysql")
        {
            LOG_INFO("MySQL 서비스 최종 상태: active_state={}, status={}",
                     service.active_state, service.status);
        }
    }

    LOG_INFO("비동기 서비스 정보 수집 완료: {} 개", infoResult.size());
    return infoResult;
}

/**
 * @brief 수집된 서비스 정보 병합
 *
 * 백그라운드에서 수집된 서비스 정보를 기존 캐시와 병합합니다.
 *
 * @param updatedServices 업데이트된 서비스 정보 벡터
 */
void ServiceCollector::mergeServiceData(const vector<ServiceInfo> &updatedServices)
{
    LOG_INFO("서비스 데이터 병합 시작: {} 개", updatedServices.size());

    // MySQL 서비스 확인
    for (const auto &service : updatedServices)
    {
        if (service.name == "mysql")
        {
            LOG_INFO("병합할 MySQL 서비스 정보: active_state={}, status={}",
                     service.active_state, service.status);
        }
    }

    // 기존 캐시와 업데이트된 서비스 정보 병합
    for (const auto &service : updatedServices)
    {
        auto it = serviceInfoCache.find(service.name);
        if (it == serviceInfoCache.end())
        {
            // 새 서비스 추가
            CachedServiceInfo cachedInfo;
            cachedInfo.info = service;
            cachedInfo.lastUpdated = chrono::system_clock::now();
            cachedInfo.needsUpdate = false;
            serviceInfoCache[service.name] = cachedInfo;

            if (service.name == "mysql")
            {
                LOG_INFO("MySQL 서비스 신규 추가됨");
            }
        }
        else if (hasServiceChanged(it->second.info, service))
        {
            // 변경된 서비스 업데이트
            // 변경 시 기존 정보 중 유지해야 할 정보 (자원 사용량 등) 보존
            auto oldMemory = it->second.info.memory_usage;
            auto oldCpu = it->second.info.cpu_usage;

            if (service.name == "mysql")
            {
                LOG_INFO("MySQL 서비스 변경 감지: 이전={}, 현재={}",
                         it->second.info.active_state, service.active_state);
            }

            it->second.info = service;

            // 상세 정보가 갱신되지 않았다면 이전 자원 정보 유지
            if (service.memory_usage == 0 && oldMemory > 0)
            {
                it->second.info.memory_usage = oldMemory;
            }
            if (service.cpu_usage == 0.0f && oldCpu > 0.0f)
            {
                it->second.info.cpu_usage = oldCpu;
            }

            it->second.lastUpdated = chrono::system_clock::now();
            it->second.needsUpdate = false;
        }
    }

    // 병합 후 MySQL 서비스 상태 확인
    auto mysqlIt = serviceInfoCache.find("mysql");
    if (mysqlIt != serviceInfoCache.end())
    {
        LOG_INFO("병합 후 MySQL 서비스 상태: active_state={}, status={}",
                 mysqlIt->second.info.active_state, mysqlIt->second.info.status);
    }

    LOG_INFO("서비스 데이터 병합 완료");
}
