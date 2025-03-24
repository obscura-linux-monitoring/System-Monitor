#pragma once

#include "collector.h"
#include "models/docker_container_info.h"
#include <curl/curl.h>
#include <vector>
#include <map>
#include <chrono>
#include <string>
#include <mutex>

using namespace std;

class DockerCollector : public Collector
{
private:
    vector<DockerContainerInfo> containers;
    void sortPorts(DockerContainerInfo &container);

    // 캐싱 메커니즘을 위한 구조체 추가
    struct StatsCache
    {
        chrono::time_point<chrono::system_clock> timestamp;
        DockerContainerInfo info;
    };

    // 컨테이너 ID를 키로 하는 캐시 맵
    map<string, StatsCache> statsCache;

    // 캐시 유효 시간 (초)
    int cacheTTLSeconds = 5;

    // curl 핸들 풀을 위한 변수
    vector<CURL *> curlHandlePool;
    mutex curlPoolMutex;

    // 캐시 및 컨테이너 목록 접근을 위한 뮤텍스 추가
    mutex containersMutex;

    // curl 핸들 관리 메서드
    void initCurlPool(size_t poolSize = 5);
    CURL *getCurlHandle();
    void returnCurlHandle(CURL *handle);

public:
    DockerCollector();
    virtual ~DockerCollector();
    void collect() override;
    vector<DockerContainerInfo> getContainers() const;

    // 캐시 설정 메서드
    void setCacheTTL(int seconds) { cacheTTLSeconds = seconds; }
    void clearCache() { statsCache.clear(); }
};