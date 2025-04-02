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

/**
 * @class DockerCollector
 * @brief Docker 컨테이너 정보를 수집하는 클래스
 *
 * Docker 데몬 API를 통해 실행 중인 컨테이너의 메타데이터와 성능 지표를 수집합니다.
 * 캐싱 메커니즘을 통해 성능을 최적화하고 curl 핸들 풀을 사용하여 API 호출을 관리합니다.
 * @see Collector
 */
class DockerCollector : public Collector
{
private:
    /**
     * @brief 수집된 Docker 컨테이너 정보를 저장하는 벡터
     */
    vector<DockerContainerInfo> containers;

    /**
     * @brief 컨테이너 포트 정보를 정렬하는 헬퍼 함수
     * @param container 포트 정보를 정렬할 컨테이너 객체
     */
    void sortPorts(DockerContainerInfo &container);

    /**
     * @brief 캐싱을 위한 컨테이너 정보 저장 구조체
     */
    struct StatsCache
    {
        /**
         * @brief 정보가 수집된 시간
         */
        chrono::time_point<chrono::system_clock> timestamp;

        /**
         * @brief 캐시된 컨테이너 정보
         */
        DockerContainerInfo info;
    };

    /**
     * @brief 컨테이너 ID를 키로 사용하는 캐시 맵
     */
    map<string, StatsCache> statsCache;

    /**
     * @brief 캐시 유효 시간 (초)
     */
    int cacheTTLSeconds = 5;

    /**
     * @brief curl 핸들 풀
     */
    vector<CURL *> curlHandlePool;

    /**
     * @brief curl 핸들 풀 접근 동기화 뮤텍스
     */
    mutex curlPoolMutex;

    /**
     * @brief 컨테이너 정보 접근 동기화 뮤텍스
     */
    mutex containersMutex;

    /**
     * @brief curl 핸들 풀 초기화 함수
     * @param poolSize 생성할 curl 핸들 수 (기본값: 5)
     */
    void initCurlPool(size_t poolSize = 5);

    /**
     * @brief 풀에서 curl 핸들을 가져오는 함수
     * @return CURL* 사용 가능한 curl 핸들
     */
    CURL *getCurlHandle();

    /**
     * @brief 사용 완료된 curl 핸들을 풀로 반환하는 함수
     * @param handle 반환할 curl 핸들
     */
    void returnCurlHandle(CURL *handle);

public:
    /**
     * @brief DockerCollector 생성자
     *
     * curl 핸들 풀을 초기화하고 Docker 데몬 연결 준비
     */
    DockerCollector();

    /**
     * @brief DockerCollector 소멸자
     *
     * curl 핸들 풀과 기타 리소스를 정리
     */
    virtual ~DockerCollector();

    /**
     * @brief Docker 컨테이너 정보 수집 메서드
     *
     * Docker 데몬 API를 호출하여 실행 중인 모든 컨테이너의 상태와
     * 성능 지표를 수집하고 내부 컨테이너 목록을 업데이트합니다.
     */
    void collect() override;

    /**
     * @brief 수집된 컨테이너 정보 접근자
     * @return vector<DockerContainerInfo> 수집된 컨테이너 정보 벡터
     */
    vector<DockerContainerInfo> getContainers() const;

    /**
     * @brief 캐시 유효 시간 설정 메서드
     * @param seconds 캐시 유효 시간 (초)
     */
    void setCacheTTL(int seconds) { cacheTTLSeconds = seconds; }

    /**
     * @brief 캐시 초기화 메서드
     */
    void clearCache() { statsCache.clear(); }
};