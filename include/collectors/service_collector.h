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

/**
 * @brief 캐싱된 서비스 정보를 저장하는 구조체
 *
 * 서비스 정보와 해당 정보의 마지막 업데이트 시간 및 업데이트 필요성을 관리한다.
 */
struct CachedServiceInfo
{
    ServiceInfo info;                                     ///< 서비스에 대한 실제 정보
    chrono::time_point<chrono::system_clock> lastUpdated; ///< 마지막으로 정보가 업데이트된 시간
    bool needsUpdate;                                     ///< 정보 업데이트 필요 여부 플래그
};

/**
 * @brief 시스템 서비스 정보를 수집하는 클래스
 *
 * Collector 클래스를 상속받아 시스템의 서비스 정보를 효율적으로 수집하고 캐싱한다.
 * 병렬 처리와 비동기 업데이트를 지원하여 성능을 최적화한다.
 */
class ServiceCollector : public Collector
{
private:
    /**
     * @brief 캐싱 메커니즘을 위한 서비스 정보 맵
     *
     * 서비스 이름을 키로 사용하여 캐싱된 서비스 정보를 저장한다.
     */
    map<string, CachedServiceInfo> serviceInfoCache;

    /**
     * @brief 수집된 서비스 정보 벡터
     *
     * 현재 시스템에서 수집된 모든 서비스 정보를 저장한다.
     */
    vector<ServiceInfo> serviceInfo;

    /**
     * @brief 캐시 접근을 동기화하기 위한 뮤텍스
     */
    mutex cacheMutex;

    /**
     * @brief 성능 최적화를 위한 현재 활성 서비스 목록
     */
    vector<ServiceInfo> currentServices;

    /**
     * @brief 결과 문자열 저장 변수
     */
    string result;

    /**
     * @brief 상세 정보 수집이 필요한 서비스 목록
     */
    vector<string> requiredDetailedServices;

    /**
     * @brief SD-Bus 연결 재사용을 위한 멤버 변수
     */
    sd_bus *m_bus = nullptr;

    /**
     * @brief 마지막 전체 업데이트 시간 추적 변수
     */
    chrono::time_point<chrono::system_clock> lastFullUpdate;

    /**
     * @brief 병렬 처리를 위한 최대 스레드 수
     */
    int maxThreads;

    /**
     * @brief 병렬 작업 관리를 위한 future 객체 벡터
     */
    vector<future<void>> futures;

    /**
     * @brief 네이티브 API 사용 여부 플래그
     */
    bool useNativeAPI;

    /**
     * @brief 비동기 업데이트를 위한 백그라운드 스레드
     */
    thread backgroundUpdateThread;

    /**
     * @brief 백그라운드 스레드 중지 플래그
     */
    atomic<bool> stopBackgroundThread;

    /**
     * @brief 서비스 데이터 접근 동기화를 위한 뮤텍스
     */
    mutex serviceDataMutex;

    /**
     * @brief 백그라운드 업데이트 작업을 수행하는 메서드
     *
     * 별도 스레드에서 실행되어 주기적으로 서비스 정보를 업데이트한다.
     */
    void backgroundUpdateTask();

    /**
     * @brief 백그라운드 업데이트 스레드 시작 메서드
     */
    void startBackgroundUpdate();

    /**
     * @brief 백그라운드 업데이트 스레드 중지 메서드
     */
    void stopBackgroundUpdate();

    /**
     * @brief 네이티브 API를 사용하여 비동기적으로 서비스 정보 수집
     *
     * @return 수집된 서비스 정보 벡터
     */
    vector<ServiceInfo> collectServicesNativeAsync();

    /**
     * @brief 모든 서비스 정보를 비동기적으로 수집
     *
     * @return 수집된 서비스 정보 벡터
     */
    vector<ServiceInfo> collectAllServicesInfoAsync();

    /**
     * @brief 업데이트된 서비스 데이터를 기존 데이터와 병합
     *
     * @param updatedServices 새로 업데이트된 서비스 정보 벡터
     */
    void mergeServiceData(const vector<ServiceInfo> &updatedServices);

    /**
     * @brief 서비스 정보 초기화 메서드
     */
    void clear();

    /**
     * @brief 기존 서비스 정보 업데이트
     *
     * @param serviceInfo 업데이트할 서비스 정보
     */
    void updateServiceInfo(const ServiceInfo &serviceInfo);

    /**
     * @brief 더 이상 존재하지 않는 서비스 제거
     *
     * @param currentServices 현재 활성화된 서비스 목록
     */
    void removeObsoleteServices(const vector<ServiceInfo> &currentServices);

    /**
     * @brief 서비스 목록 수집 메서드
     *
     * @param result 수집된 서비스 목록을 저장할 벡터 참조
     */
    void collectServiceList(vector<ServiceInfo> &result);

    /**
     * @brief 특정 서비스의 상세 정보 수집
     *
     * @param service 정보를 수집할 서비스 객체 참조
     * @param forceUpdate 강제 업데이트 여부
     */
    void collectServiceDetails(ServiceInfo &service, bool forceUpdate = false);

    /**
     * @brief 서비스의 리소스 사용량 수집
     *
     * @param service 정보를 수집할 서비스 객체 참조
     */
    void collectResourceUsage(ServiceInfo &service);

    /**
     * @brief PID별 리소스 사용량 수집
     *
     * @param service 정보를 수집할 서비스 객체 참조
     * @param pid 대상 프로세스 ID
     */
    void collectResourceUsageByPID(ServiceInfo &service, const string &pid);

    /**
     * @brief 모든 서비스 정보 수집 메서드
     */
    void collectAllServicesInfo();

    /**
     * @brief 서비스 정보 변경 여부 확인
     *
     * @param oldInfo 이전 서비스 정보
     * @param newInfo 새 서비스 정보
     * @return 변경 여부 (true: 변경됨, false: 변경 없음)
     */
    bool hasServiceChanged(const ServiceInfo &oldInfo, const ServiceInfo &newInfo) const;

    /**
     * @brief 캐시된 서비스 정보 업데이트
     *
     * @param name 서비스 이름
     * @param info 업데이트할 서비스 정보
     */
    void updateCachedInfo(const string &name, const ServiceInfo &info);

    /**
     * @brief 네이티브 API를 사용하여 서비스 정보 수집
     */
    void collectUsingNativeAPI();

    /**
     * @brief 네이티브 API를 사용하여 서비스 상세 정보 수집
     *
     * @param service 정보를 수집할 서비스 객체 참조
     */
    void collectServiceDetailsNative(ServiceInfo &service);

    /**
     * @brief 기본 서비스 정보 수집
     */
    void collectBasicServiceInfo();

    /**
     * @brief SD-Bus를 통해 여러 속성 값을 한 번에 가져오기
     *
     * @param bus SD-Bus 객체 포인터
     * @param service_path 서비스 경로
     * @param properties 속성 이름 배열
     * @param count 속성 개수
     * @return 성공 여부
     */
    bool GetMultipleProperties(sd_bus *bus, const char *service_path,
                               const char *properties[], int count);

    /**
     * @brief 모든 서비스의 활성화 상태를 한 번에 수집
     *
     * @param bus SD-Bus 객체 포인터
     * @param enabledStates 활성화 상태를 저장할 맵 참조
     */
    void collectAllEnabledStatesAtOnce(sd_bus *bus, map<string, bool> &enabledStates);

    /**
     * @brief 변경된 서비스만 업데이트
     */
    void updateChangedServicesOnly();

public:
    /**
     * @brief 생성자
     *
     * @param threads 사용할 스레드 수 (기본값: 4)
     * @param useNative 네이티브 API 사용 여부 (기본값: false)
     */
    ServiceCollector(int threads = 4, bool useNative = false);

    /**
     * @brief 서비스 정보 수집 메서드 (Collector 클래스에서 상속)
     */
    void collect() override;

    /**
     * @brief 수집된 서비스 정보 반환
     *
     * @return 서비스 정보 벡터 복사본
     */
    vector<ServiceInfo> getServiceInfo() const;

    /**
     * @brief 수집된 서비스 정보 참조 반환 (성능 최적화용)
     *
     * @return 서비스 정보 벡터 상수 참조
     */
    const vector<ServiceInfo> &getServiceInfoRef() const;

    /**
     * @brief 네이티브 API 사용 여부 설정
     *
     * @param use 사용 여부 (true: 사용, false: 미사용)
     */
    void setUseNativeAPI(bool use);

    /**
     * @brief 소멸자
     *
     * 리소스 해제 및 백그라운드 스레드 종료를 담당한다.
     */
    ~ServiceCollector();

    /**
     * @brief 상세 정보 수집이 필요한 서비스 추가
     *
     * @param serviceName 서비스 이름
     */
    void addRequiredDetailedService(const string &serviceName);

    /**
     * @brief 상세 정보 수집 필요 서비스 목록 초기화
     */
    void clearRequiredDetailedServices();
};