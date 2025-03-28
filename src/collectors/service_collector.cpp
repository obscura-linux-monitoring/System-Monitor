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

// 생성자 - 병렬 처리 및 API 선택 초기화
ServiceCollector::ServiceCollector(int threads, bool useNative) : maxThreads(max(1, threads)),
                                                                  useNativeAPI(useNative),
                                                                  stopBackgroundThread(false)
{
    // 백그라운드 업데이트 시작
    startBackgroundUpdate();
}

void ServiceCollector::setUseNativeAPI(bool use)
{
    useNativeAPI = use;
}

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

    // 결과 벡터 업데이트 (필요시에만)
    if (serviceInfo.empty() || serviceInfo.size() != serviceInfoCache.size())
    {
        serviceInfo.clear();
        for (const auto &pair : serviceInfoCache)
        {
            serviceInfo.push_back(pair.second.info);
        }
    }
}

// 모든 서비스 정보를 최적화된 방식으로 수집
void ServiceCollector::collectAllServicesInfo()
{
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
    string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
    }
    pclose(pipe);

    // 현재 시스템의 서비스 목록 수집
    vector<ServiceInfo> currentServices;

    // JSON 파싱 (간단한 구현으로 대체 - 실제로는 JSON 라이브러리 사용 권장)
    // 여기서는 파싱 코드를 생략하고, 기존 방식으로 진행합니다
    collectServiceList(currentServices);

    // 2. 변경 감지 및 캐싱 메커니즘
    {
        lock_guard<mutex> lock(cacheMutex);

        // 새 서비스 추가 및 변경 서비스 표시
        for (auto &service : currentServices)
        {
            auto it = serviceInfoCache.find(service.name);
            if (it == serviceInfoCache.end())
            {
                // 새 서비스 - 캐시에 추가
                CachedServiceInfo cachedInfo;
                cachedInfo.info = service;
                cachedInfo.lastUpdated = chrono::system_clock::now();
                cachedInfo.needsUpdate = true;
                serviceInfoCache[service.name] = cachedInfo;
            }
            else if (hasServiceChanged(it->second.info, service))
            {
                // 변경된 서비스 - 업데이트 필요 표시
                it->second.info.status = service.status;
                it->second.info.enabled = service.enabled;
                it->second.needsUpdate = true;
            }
        }

        // 삭제된 서비스 제거
        auto it = serviceInfoCache.begin();
        while (it != serviceInfoCache.end())
        {
            if (none_of(currentServices.begin(), currentServices.end(),
                        [&it](const ServiceInfo &info)
                        { return info.name == it->first; }))
            {
                it = serviceInfoCache.erase(it);
            }
            else
            {
                ++it;
            }
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

            ServiceInfo serviceInfo;
            {
                lock_guard<mutex> lock(cacheMutex);
                auto it = serviceInfoCache.find(serviceName);
                if (it != serviceInfoCache.end())
                {
                    serviceInfo = it->second.info;
                }
                else
                {
                    continue; // 서비스가 이미 삭제됨
                }
            }

            // 락 없이 작업 시작
            futures.push_back(async(launch::async,
                                    [this, serviceName, serviceInfo]() mutable
                                    {
                                        collectServiceDetails(serviceInfo);

                                        lock_guard<mutex> lock(cacheMutex);
                                        auto it = serviceInfoCache.find(serviceName);
                                        if (it != serviceInfoCache.end())
                                        {
                                            it->second.info = serviceInfo;
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

// 두 서비스 정보 간의 중요한 변경 여부 확인
bool ServiceCollector::hasServiceChanged(const ServiceInfo &oldInfo, const ServiceInfo &newInfo) const
{
    return (oldInfo.status != newInfo.status ||
            oldInfo.enabled != newInfo.enabled ||
            oldInfo.active_state != newInfo.active_state);
}

// 기존 서비스 목록 수집 메서드 (최적화)
void ServiceCollector::collectServiceList(vector<ServiceInfo> &result)
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

            result.push_back(info);
        }
    }

    int status = pclose(pipe);
    if (status == -1)
    {
        LOG_ERROR("서비스 목록 명령 종료 실패");
    }
}

// 서비스 상세 정보 수집 (최적화)
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

// 자원 사용량 수집 (기존과 동일)
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

// 캐시 정보 업데이트
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

void ServiceCollector::removeObsoleteServices(const vector<ServiceInfo> &currentServices)
{
    // 현재 시스템에 존재하지 않는 서비스 제거
    serviceInfo.erase(
        remove_if(serviceInfo.begin(), serviceInfo.end(),
                  [&currentServices](const ServiceInfo &info)
                  {
                      return find_if(currentServices.begin(), currentServices.end(),
                                     [&info](const ServiceInfo &current)
                                     {
                                         return current.name == info.name;
                                     }) == currentServices.end();
                  }),
        serviceInfo.end());
}

void ServiceCollector::clear()
{
    serviceInfo.clear();
    lock_guard<mutex> lock(cacheMutex);
    serviceInfoCache.clear();
}

vector<ServiceInfo> ServiceCollector::getServiceInfo() const
{
    return serviceInfo;
}

const vector<ServiceInfo> &ServiceCollector::getServiceInfoRef() const
{
    return serviceInfo;
}

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

// 네이티브 API를 사용한 정보 수집 메서드
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
    vector<ServiceInfo> currentServices;

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

            currentServices.push_back(info);
        }

        sd_bus_message_exit_container(reply);
    }

    sd_bus_message_unref(reply);

    // 서비스 활성화 상태 가져오기
    map<string, bool> enabledStates;
    collectAllEnabledStatesAtOnce(bus, enabledStates);

    for (auto &service : currentServices)
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

        for (const auto &service : currentServices)
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

// 네이티브 API를 사용한 서비스 상세 정보 수집
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
            // 여러 속성을 한 번에 가져오기
            const char *properties[] = {"Type", "MainPID", "MemoryCurrent", "CPUUsageNSec"};

            // 여러 속성을 단일 호출로 가져오기
            if (GetMultipleProperties(bus, service_path, properties, 4))
            {
                // 결과 처리
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

// 모든 서비스의 활성화 상태를 한 번에 수집
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
            LOG_ERROR("유닛 파일 정보 읽기 실패: {}", strerror(-r));
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

// 기본 서비스 정보만 수집하는 메서드 구현
void ServiceCollector::collectBasicServiceInfo()
{
    auto now = chrono::system_clock::now();
    static auto lastFullUpdate = now;

    // 5초 이내에는 변경된 서비스만 업데이트
    bool partialUpdate = chrono::duration_cast<chrono::seconds>(
                             now - lastFullUpdate)
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
        lastFullUpdate = now;
    }
}

// 변경된 서비스만 업데이트하는 메서드
void ServiceCollector::updateChangedServicesOnly()
{
    // systemctl list-units --state=changed로 변경된 서비스만 확인
    FILE *pipe = popen("systemctl list-units --state=changed --type=service --no-legend --no-pager 2>/dev/null", "r");
    if (!pipe)
    {
        LOG_ERROR("변경된 서비스 목록 명령 실행 실패");
        return;
    }

    vector<string> changedServices;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        string line(buffer);
        istringstream iss(line);
        string name;

        if (iss >> name)
        {
            // .service 확장자 제거
            const string suffix = ".service";
            if (name.length() >= suffix.length() &&
                name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0)
            {
                name = name.substr(0, name.length() - suffix.length());
            }
            changedServices.push_back(name);
        }
    }
    pclose(pipe);

    // 캐시에 변경 플래그 설정
    {
        lock_guard<mutex> lock(cacheMutex);
        for (const auto &name : changedServices)
        {
            auto it = serviceInfoCache.find(name);
            if (it != serviceInfoCache.end())
            {
                it->second.needsUpdate = true;
            }
        }
    }

    // 변경된 서비스만 업데이트
    for (const auto &name : changedServices)
    {
        ServiceInfo serviceInfo;
        {
            lock_guard<mutex> lock(cacheMutex);
            auto it = serviceInfoCache.find(name);
            if (it != serviceInfoCache.end())
            {
                serviceInfo = it->second.info;
            }
            else
            {
                continue;
            }
        }

        collectServiceDetails(serviceInfo, true);
        updateCachedInfo(name, serviceInfo);
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

// 여러 속성을 한 번에 가져오는 도우미 메서드
bool ServiceCollector::GetMultipleProperties(sd_bus *bus, const char *service_path,
                                             const char *properties[], int count)
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

    bool found[count] = {false};
    int foundCount = 0;

    // 모든 속성 순회
    while (foundCount < count &&
           sd_bus_message_enter_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv") > 0)
    {
        const char *prop_name;
        r = sd_bus_message_read_basic(reply, SD_BUS_TYPE_STRING, &prop_name);

        if (r >= 0)
        {
            // 찾는 속성인지 확인
            for (int i = 0; i < count; i++)
            {
                if (!found[i] && strcmp(prop_name, properties[i]) == 0)
                {
                    // 일치하는 속성 찾음, 값 파싱
                    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_VARIANT, NULL);
                    if (r >= 0)
                    {
                        // 타입에 따라 적절히 파싱
                        // (여기서는 단순화를 위해 기본 타입만 처리)
                        found[i] = true;
                        foundCount++;
                    }
                    sd_bus_message_exit_container(reply); // variant 컨테이너 종료
                    break;
                }
            }
        }

        sd_bus_message_exit_container(reply); // dict_entry 컨테이너 종료
    }

    sd_bus_message_exit_container(reply); // array 컨테이너 종료
    sd_bus_message_unref(reply);

    return foundCount > 0;
}

// 소멸자 - sd-bus 객체 해제
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

// 상세 정보 수집이 필요한 서비스 추가
void ServiceCollector::addRequiredDetailedService(const string &serviceName)
{
    if (find(requiredDetailedServices.begin(), requiredDetailedServices.end(), serviceName) == requiredDetailedServices.end())
    {
        requiredDetailedServices.push_back(serviceName);
    }
}

// 상세 정보 수집 목록 초기화
void ServiceCollector::clearRequiredDetailedServices()
{
    requiredDetailedServices.clear();
}

// 백그라운드 태스크 실행
void ServiceCollector::backgroundUpdateTask()
{
    while (!stopBackgroundThread)
    {
        {
            // 전체 서비스 정보 업데이트 - 메인 스레드와 별도로 실행
            vector<ServiceInfo> updatedServices;

            if (useNativeAPI)
            {
                updatedServices = collectServicesNativeAsync();
            }
            else
            {
                updatedServices = collectAllServicesInfoAsync();
            }

            // 결과를 메인 데이터 구조에 안전하게 병합
            lock_guard<mutex> lock(serviceDataMutex);
            mergeServiceData(updatedServices);
        }

        // 5초 간격으로 업데이트
        for (int i = 0; i < 50 && !stopBackgroundThread; i++)
        {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
}

void ServiceCollector::startBackgroundUpdate()
{
    stopBackgroundThread = false;
    backgroundUpdateThread = thread(&ServiceCollector::backgroundUpdateTask, this);
}

void ServiceCollector::stopBackgroundUpdate()
{
    stopBackgroundThread = true;
    if (backgroundUpdateThread.joinable())
    {
        backgroundUpdateThread.join();
    }
}

// 비동기 네이티브 API 사용 서비스 수집
vector<ServiceInfo> ServiceCollector::collectServicesNativeAsync()
{
    vector<ServiceInfo> result;

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
        return result;
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
        return result;
    }

    // 응답 파싱 시작
    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (r < 0)
    {
        LOG_ERROR("비동기 메시지 컨테이너 진입 실패: {}", strerror(-r));
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        return result;
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

            result.push_back(info);
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
    for (auto &service : result)
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
    return result;
}

// 비동기 서비스 정보 수집
vector<ServiceInfo> ServiceCollector::collectAllServicesInfoAsync()
{
    vector<ServiceInfo> result;

    // 모든 서비스 기본 정보를 한 번에 가져오기
    string cmd = "systemctl list-unit-files --type=service --no-pager --no-legend 2>/dev/null";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        LOG_ERROR("비동기 서비스 목록 명령 실행 실패");
        return result;
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

            result.push_back(info);
        }
    }
    pclose(pipe);

    // 중요 서비스 상태 정보 추가 수집 (10개 제한)
    int processedCount = 0;
    const int maxServices = 10; // 백그라운드에서는 일부 중요 서비스만 처리

    for (auto &service : result)
    {
        if (processedCount >= maxServices)
            break;

        // 우선순위가 높은 서비스 식별 (예: systemd, ssh, 네트워크 관련)
        if (service.name.find("systemd") != string::npos ||
            service.name.find("ssh") != string::npos ||
            service.name.find("network") != string::npos ||
            service.name.find("boot") != string::npos)
        {

            // 간소화된 상세 정보 수집
            string detailCmd = "systemctl show " + service.name +
                               ".service -p Type,LoadState,ActiveState,SubState 2>/dev/null";

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
                    }
                }
                pclose(detailPipe);
                processedCount++;
            }
        }
    }

    return result;
}

// 수집된 서비스 정보 병합
void ServiceCollector::mergeServiceData(const vector<ServiceInfo> &updatedServices)
{
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
        }
        else if (hasServiceChanged(it->second.info, service))
        {
            // 변경된 서비스 업데이트
            // 변경 시 기존 정보 중 유지해야 할 정보 (자원 사용량 등) 보존
            auto oldMemory = it->second.info.memory_usage;
            auto oldCpu = it->second.info.cpu_usage;

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

    // 삭제된 서비스 처리
    auto now = chrono::system_clock::now();
    for (auto it = serviceInfoCache.begin(); it != serviceInfoCache.end();)
    {
        // 업데이트된 서비스 목록에 없는 서비스 확인
        if (none_of(updatedServices.begin(), updatedServices.end(),
                    [&it](const ServiceInfo &service)
                    {
                        return service.name == it->first;
                    }))
        {

            // 30초 이상 업데이트되지 않은 서비스는 삭제
            auto elapsed = chrono::duration_cast<chrono::seconds>(
                               now - it->second.lastUpdated)
                               .count();

            if (elapsed > 30)
            {
                it = serviceInfoCache.erase(it);
            }
            else
            {
                ++it;
            }
        }
        else
        {
            ++it;
        }
    }
}
