#include "utils/system_metrics_utils.h"

#include <nlohmann/json.hpp>
#include <string>
#include <sstream>
#include <iomanip>

using namespace std;
using json = nlohmann::json;

/**
 * @brief CPU 코어 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param core 변환할 CPU 코어 정보 구조체
 */
void to_json(json &j, const CpuCoreInfo &core)
{
    j = {
        {"id", core.id},
        {"usage", core.usage},
        {"temperature", core.temperature}};
}

/**
 * @brief 시스템 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param system 변환할 시스템 정보 구조체
 */
void to_json(json &j, const SystemInfo &system)
{
    j = {
        {"hostname", system.hostname},
        {"os_name", system.os_name},
        {"os_version", system.os_version},
        {"os_kernel_version", system.os_kernel_version},
        {"os_architecture", system.os_architecture},
        {"uptime", system.uptime},
        {"boot_time", system.boot_time},
        {"total_processes", system.total_processes},
        {"total_threads", system.total_threads},
        {"total_file_descriptors", system.total_file_descriptors}};
}

/**
 * @brief CPU 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param cpu 변환할 CPU 정보 구조체
 */
void to_json(json &j, const CpuInfo &cpu)
{
    j = {
        {"model", cpu.model},
        {"vendor", cpu.vendor},
        {"architecture", cpu.architecture},
        {"usage", cpu.usage},
        {"temperature", cpu.temperature},
        {"total_cores", cpu.total_cores},
        {"total_logical_cores", cpu.total_logical_cores},
        {"is_hyperthreading", cpu.is_hyperthreading},
        {"clock_speed", cpu.clock_speed},
        {"cache_size", cpu.cache_size},
        {"cores", cpu.cores},
        {"has_vmx", cpu.has_vmx},
        {"has_svm", cpu.has_svm},
        {"has_avx", cpu.has_avx},
        {"has_avx2", cpu.has_avx2},
        {"has_neon", cpu.has_neon},
        {"has_sve", cpu.has_sve},
        {"l1_cache_size", cpu.l1_cache_size},
        {"l2_cache_size", cpu.l2_cache_size},
        {"l3_cache_size", cpu.l3_cache_size},
        {"base_clock_speed", cpu.base_clock_speed},
        {"max_clock_speed", cpu.max_clock_speed},
        {"min_clock_speed", cpu.min_clock_speed}};
}

/**
 * @brief 메모리 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param memory 변환할 메모리 정보 구조체
 */
void to_json(json &j, const MemoryInfo &memory)
{
    j = {
        {"total", memory.total},
        {"used", memory.used},
        {"free", memory.free},
        {"cached", memory.cached},
        {"buffers", memory.buffers},
        {"available", memory.available},
        {"swap_total", memory.swap_total},
        {"swap_used", memory.swap_used},
        {"swap_free", memory.swap_free},
        {"usage_percent", memory.usage_percent},
        {"data_rate", memory.data_rate},
        {"total_slot_count", memory.total_slot_count},
        {"using_slot_count", memory.using_slot_count},
        {"form_factor", memory.form_factor},
        {"paged_pool_size", memory.paged_pool_size},
        {"non_paged_pool_size", memory.non_paged_pool_size}};
}

/**
 * @brief 입출력 통계 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param io 변환할 입출력 통계 정보 구조체
 */
void to_json(json &j, const IoStats &io)
{
    j = {
        {"reads", io.reads},
        {"writes", io.writes},
        {"read_bytes", io.read_bytes},
        {"write_bytes", io.write_bytes},
        {"read_time", io.read_time},
        {"write_time", io.write_time},
        {"io_time", io.io_time},
        {"io_in_progress", io.io_in_progress},
        {"reads_per_sec", io.reads_per_sec},
        {"writes_per_sec", io.writes_per_sec},
        {"read_bytes_per_sec", io.read_bytes_per_sec},
        {"write_bytes_per_sec", io.write_bytes_per_sec},
        {"error_flag", io.error_flag}};
}

/**
 * @brief 디스크 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param disk 변환할 디스크 정보 구조체
 */
void to_json(json &j, const DiskInfo &disk)
{
    j = {
        {"device", disk.device},
        {"mount_point", disk.mount_point},
        {"filesystem_type", disk.filesystem_type},
        {"total", disk.total},
        {"used", disk.used},
        {"free", disk.free},
        {"usage_percent", disk.usage_percent},
        {"inodes_total", disk.inodes_total},
        {"inodes_used", disk.inodes_used},
        {"inodes_free", disk.inodes_free},
        {"io_stats", disk.io_stats},
        {"error_flag", disk.error_flag},
        {"error_message", disk.error_message}};
}

/**
 * @brief 네트워크 인터페이스 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param network 변환할 네트워크 인터페이스 정보 구조체
 */
void to_json(json &j, const NetworkInterface &network)
{
    j = {
        {"interface", network.interface},
        {"ip", network.ip},
        {"mac", network.mac},
        {"status", network.status},
        {"speed", network.speed},
        {"mtu", network.mtu},
        {"rx_bytes", network.rx_bytes},
        {"tx_bytes", network.tx_bytes},
        {"rx_packets", network.rx_packets},
        {"tx_packets", network.tx_packets},
        {"rx_errors", network.rx_errors},
        {"tx_errors", network.tx_errors},
        {"rx_dropped", network.rx_dropped},
        {"tx_dropped", network.tx_dropped},
        {"rx_bytes_per_sec", network.rx_bytes_per_sec},
        {"tx_bytes_per_sec", network.tx_bytes_per_sec}};
}

/**
 * @brief 프로세스 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param process 변환할 프로세스 정보 구조체
 */
void to_json(json &j, const ProcessInfo &process)
{
    j = {
        {"pid", process.pid},
        {"ppid", process.ppid},
        {"name", process.name},
        {"status", process.status},
        {"cpu_usage", process.cpu_usage},
        {"memory_rss", process.memory_rss},
        {"memory_vsz", process.memory_vsz},
        {"threads", process.threads},
        {"user", process.user},
        {"command", process.command},
        {"start_time", process.start_time},
        {"cpu_time", process.cpu_time},
        {"io_read_bytes", process.io_read_bytes},
        {"io_write_bytes", process.io_write_bytes},
        {"open_files", process.open_files},
        {"nice", process.nice}};
}

/**
 * @brief 도커 컨테이너 건강 상태 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param health 변환할 도커 컨테이너 건강 상태 정보 구조체
 */
void to_json(json &j, const DockerContainerHealth &health)
{
    j = {
        {"status", health.status},
        {"failing_streak", health.failing_streak},
        {"last_check_output", health.last_check_output}};
}

/**
 * @brief 도커 컨테이너 포트 바인딩 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param port 변환할 도커 컨테이너 포트 바인딩 정보 구조체
 */
void to_json(json &j, const DockerContainerPort &port)
{
    j = {
        {"container_port", port.container_port},
        {"host_port", port.host_port},
        {"protocol", port.protocol}};
}

/**
 * @brief 도커 컨테이너 네트워크 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param network 변환할 도커 컨테이너 네트워크 정보 구조체
 */
void to_json(json &j, const DockerContainerNetwork &network)
{
    j = {
        {"network_name", network.network_name},
        {"network_ip", network.network_ip},
        {"network_mac", network.network_mac},
        {"network_rx_bytes", network.network_rx_bytes},
        {"network_tx_bytes", network.network_tx_bytes}};
}

/**
 * @brief 도커 컨테이너 볼륨 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param volume 변환할 도커 컨테이너 볼륨 정보 구조체
 */
void to_json(json &j, const DockerContainerVolume &volume)
{
    j = {
        {"source", volume.source},
        {"destination", volume.destination},
        {"mode", volume.mode},
        {"type", volume.type}};
}

/**
 * @brief 도커 컨테이너 환경 변수 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param env 변환할 도커 컨테이너 환경 변수 정보 구조체
 */
void to_json(json &j, const DockerContainerEnv &env)
{
    j = {
        {"key", env.key},
        {"value", env.value}};
}

/**
 * @brief 도커 컨테이너 라벨 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param label 변환할 도커 컨테이너 라벨 정보 구조체
 */
void to_json(json &j, const DockerContainerLabel &label)
{
    j = {
        {"label_key", label.label_key},
        {"label_value", label.label_value}};
}

/**
 * @brief 도커 컨테이너 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param docker 변환할 도커 컨테이너 정보 구조체
 */
void to_json(json &j, const DockerContainerInfo &docker)
{
    j = {
        {"container_id", docker.container_id},
        {"container_name", docker.container_name},
        {"container_image", docker.container_image},
        {"container_status", docker.container_status},
        {"container_created", docker.container_created},
        {"container_health", docker.container_health},
        {"container_ports", docker.container_ports},
        {"container_network", docker.container_network},
        {"container_volumes", docker.container_volumes},
        {"container_env", docker.container_env},
        {"cpu_usage", docker.cpu_usage},
        {"memory_usage", docker.memory_usage},
        {"memory_limit", docker.memory_limit},
        {"memory_percent", docker.memory_percent},
        {"block_read", docker.block_read},
        {"block_write", docker.block_write},
        {"labels", docker.labels},
        {"command", docker.command},
        {"network_rx_bytes", docker.network_rx_bytes},
        {"network_tx_bytes", docker.network_tx_bytes},
        {"pids", docker.pids},
        {"restarts", docker.restarts}};
}

/**
 * @brief 서비스 정보를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param service 변환할 서비스 정보 구조체
 */
void to_json(json &j, const ServiceInfo &service)
{
    j = {
        {"name", service.name},
        {"status", service.status},
        {"enabled", service.enabled},
        {"type", service.type},
        {"load_state", service.load_state},
        {"active_state", service.active_state},
        {"sub_state", service.sub_state},
        {"memory_usage", service.memory_usage},
        {"cpu_usage", service.cpu_usage}};
}

/**
 * @brief 시스템 메트릭스 전체 구조를 JSON으로 변환
 *
 * @param j 변환된 정보가 저장될 JSON 객체
 * @param metrics 변환할 시스템 메트릭스 구조체
 */
void to_json(json &j, const SystemMetrics &metrics)
{
    j = {
        {"user_id", metrics.user_id},
        {"key", metrics.key},
        {"timestamp", metrics.timestamp},
        {"system", metrics.system},
        {"cpu", metrics.cpu},
        {"memory", metrics.memory},
        {"disk", metrics.disk},
        {"network", metrics.network},
        {"processes", metrics.process},
        {"containers", metrics.docker},
        {"services", metrics.services}};
}

/**
 * @brief 시스템 메트릭스를 JSON 문자열로 변환
 *
 * 시스템 메트릭스 구조체를 받아 JSON 형식의 문자열로 직렬화합니다.
 *
 * @param metrics 변환할 시스템 메트릭스 구조체
 * @return 시스템 메트릭스 정보가 담긴 JSON 문자열
 */
string SystemMetricsUtil::toJson(const SystemMetrics &metrics)
{
    json j;
    to_json(j, metrics);
    return j.dump();
}