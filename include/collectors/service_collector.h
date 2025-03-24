#pragma once
#include "collector.h"
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <future>
#include <chrono>
#include <functional>
#include <atomic>
#include "models/service_info.h"
#include <systemd/sd-bus.h>

using namespace std;

// 캐싱된 서비스 정보를 저장하는 구조체
struct CachedServiceInfo
{
    ServiceInfo info;
    chrono::time_point<chrono::system_clock> lastUpdated;
    bool needsUpdate;
};

class ServiceCollector : public Collector
{
private:
    // 캐싱 메커니즘을 위한 서비스 정보 맵
    map<string, CachedServiceInfo> serviceInfoCache;
    vector<ServiceInfo> serviceInfo;
    mutex cacheMutex;

    // 성능 최적화를 위한 추가 멤버 변수
    vector<ServiceInfo> currentServices;
    string result;
    vector<string> requiredDetailedServices;

    // SD-Bus 재사용을 위한 멤버 변수
    sd_bus *m_bus = nullptr;

    // 마지막 전체 업데이트 시간 추적
    chrono::time_point<chrono::system_clock> lastFullUpdate;

    // 병렬 처리를 위한 스레드 풀 관련 변수
    int maxThreads;
    vector<future<void>> futures;

    // 네이티브 API 사용 여부
    bool useNativeAPI;

    // 비동기 업데이트를 위한 멤버 추가
    thread backgroundUpdateThread;
    atomic<bool> stopBackgroundThread;
    mutex serviceDataMutex;

    // 비동기 업데이트 메서드들
    void backgroundUpdateTask();
    void startBackgroundUpdate();
    void stopBackgroundUpdate();
    vector<ServiceInfo> collectServicesNativeAsync();
    vector<ServiceInfo> collectAllServicesInfoAsync();
    void mergeServiceData(const vector<ServiceInfo> &updatedServices);

    // 기존 메서드들
    void clear();
    void updateServiceInfo(const ServiceInfo &serviceInfo);                  // 이미 있는 service일 경우 정보 업데이트
    void removeObsoleteServices(const vector<ServiceInfo> &currentServices); // 새 메서드

    // 최적화된 메서드들
    void collectServiceList(vector<ServiceInfo> &result); // 변경된 시그니처
    void collectServiceDetails(ServiceInfo &service, bool forceUpdate = false);
    void collectResourceUsage(ServiceInfo &service);
    void collectResourceUsageByPID(ServiceInfo &service, const string &pid);

    // 새로 추가된 최적화 메서드들
    void collectAllServicesInfo();
    bool hasServiceChanged(const ServiceInfo &oldInfo, const ServiceInfo &newInfo) const;
    void updateCachedInfo(const string &name, const ServiceInfo &info);

    // 네이티브 API 관련 메서드들
    void collectUsingNativeAPI();
    void collectServiceDetailsNative(ServiceInfo &service);

    // 새로 추가될 메서드들
    void collectBasicServiceInfo();
    bool GetMultipleProperties(sd_bus *bus, const char *service_path,
                               const char *properties[], int count);
    void collectAllEnabledStatesAtOnce(sd_bus *bus, map<string, bool> &enabledStates);
    void updateChangedServicesOnly();

public:
    ServiceCollector(int threads = 4, bool useNative = false);
    void collect() override;
    vector<ServiceInfo> getServiceInfo() const;
    const vector<ServiceInfo> &getServiceInfoRef() const;
    void setUseNativeAPI(bool use);
    ~ServiceCollector();
    void addRequiredDetailedService(const string &serviceName);
    void clearRequiredDetailedServices();
};