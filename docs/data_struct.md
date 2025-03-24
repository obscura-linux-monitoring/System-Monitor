# Linux 시스템 정보 수집 구조 설명

## 기본 구조
| 필드 | 설명 |
|------|------|
| `key` | 서버를 고유하게 식별하는 해시 키 |
| `timestamp` | 데이터가 수집된 시간 (ISO 8601 형식 권장) |

## 시스템 일반 정보
| 필드 | 설명 |
|------|------|
| `system.hostname` | 시스템의 호스트명 |
| `system.os_name` | 운영체제 이름 (예: Ubuntu, CentOS) |
| `system.os_version` | 운영체제 버전 (예: 22.04, 9.0) |
| `system.kernel_version` | Linux 커널 버전 (예: 5.15.0-91-generic) |
| `system.architecture` | 시스템 아키텍처 (예: x86_64, aarch64) |
| `system.uptime` | 시스템 가동 시간 (초 단위) |
| `system.boot_time` | 시스템 부팅 시간 (타임스탬프) |
| `system.load_average.1min` | 1분 평균 시스템 부하 |
| `system.load_average.5min` | 5분 평균 시스템 부하 |
| `system.load_average.15min` | 15분 평균 시스템 부하 |

## CPU 정보
| 필드 | 설명 |
|------|------|
| `cpu.model` | CPU 모델명 (예: Intel(R) Xeon(R) CPU E5-2680 v3) |
| `cpu.architecture` | CPU 아키텍처 (예: x86_64, ARM) |
| `cpu.usage` | 전체 CPU 사용률 (퍼센트) |
| `cpu.temperature` | CPU 전체 온도 (섭씨) |
| `cpu.total_cores` | 물리적 CPU 코어 수 |
| `cpu.total_logical_cores` | 논리적 CPU 코어 수 (하이퍼스레딩 포함) |
| `cpu.cores[].id` | 코어 ID |
| `cpu.cores[].usage` | 개별 코어 사용률 (퍼센트) |
| `cpu.cores[].speed` | 개별 코어 클럭 속도 (GHz) |
| `cpu.cores[].temperature` | 개별 코어 온도 (섭씨) |

## 메모리 정보
| 필드 | 설명 |
|------|------|
| `memory.total` | 총 물리적 메모리 크기 (바이트) |
| `memory.used` | 사용 중인 메모리 크기 (바이트) |
| `memory.free` | 사용 가능한 메모리 크기 (바이트) |
| `memory.cached` | 캐시로 사용 중인 메모리 크기 (바이트) |
| `memory.buffers` | 버퍼로 사용 중인 메모리 크기 (바이트) |
| `memory.available` | 실제로 사용 가능한 메모리 크기 (free + cached + buffers) (바이트) |
| `memory.swap_total` | 총 스왑 메모리 크기 (바이트) |
| `memory.swap_used` | 사용 중인 스왑 메모리 크기 (바이트) |
| `memory.swap_free` | 사용 가능한 스왑 메모리 크기 (바이트) |
| `memory.usage_percent` | 메모리 사용률 (퍼센트) |

## 디스크 정보
| 필드 | 설명 |
|------|------|
| `disk[].device` | 디스크 장치 이름 (예: /dev/sda1) |
| `disk[].mount_point` | 디스크 마운트 위치 (예: /home) |
| `disk[].filesystem_type` | 파일 시스템 유형 (예: ext4, xfs) |
| `disk[].total` | 총 디스크 공간 (바이트) |
| `disk[].used` | 사용 중인 디스크 공간 (바이트) |
| `disk[].free` | 사용 가능한 디스크 공간 (바이트) |
| `disk[].usage_percent` | 디스크 사용률 (퍼센트) |
| `disk[].inodes_total` | 총 inode 수 |
| `disk[].inodes_used` | 사용 중인 inode 수 |
| `disk[].inodes_free` | 사용 가능한 inode 수 |
| `disk[].io_stats.reads` | 디스크 읽기 작업 수 |
| `disk[].io_stats.writes` | 디스크 쓰기 작업 수 |
| `disk[].io_stats.read_bytes` | 읽은 바이트 수 |
| `disk[].io_stats.write_bytes` | 쓴 바이트 수 |
| `disk[].io_stats.read_time` | 읽기 작업에 소요된 시간 (밀리초) |
| `disk[].io_stats.write_time` | 쓰기 작업에 소요된 시간 (밀리초) |
| `disk[].io_stats.io_time` | I/O 작업에 소요된 총 시간 (밀리초) |
| `disk[].io_stats.io_in_progress` | 진행 중인 I/O 작업 수 |

## 네트워크 정보
| 필드 | 설명 |
|------|------|
| `network[].interface` | 네트워크 인터페이스 이름 (예: eth0, ens33) |
| `network[].ip` | IP 주소 |
| `network[].mac` | MAC 주소 |
| `network[].status` | 인터페이스 상태 (up/down) |
| `network[].speed` | 인터페이스 속도 (Mbps) |
| `network[].mtu` | Maximum Transmission Unit 크기 (바이트) |
| `network[].rx_bytes` | 받은 총 바이트 수 |
| `network[].tx_bytes` | 보낸 총 바이트 수 |
| `network[].rx_packets` | 받은 패킷 수 |
| `network[].tx_packets` | 보낸 패킷 수 |
| `network[].rx_errors` | 수신 오류 수 |
| `network[].tx_errors` | 전송 오류 수 |
| `network[].rx_dropped` | 수신 중 드롭된 패킷 수 |
| `network[].tx_dropped` | 전송 중 드롭된 패킷 수 |
| `network[].rx_bytes_per_sec` | 초당 받은 바이트 수 |
| `network[].tx_bytes_per_sec` | 초당 보낸 바이트 수 |

## 프로세스 정보
| 필드 | 설명 |
|------|------|
| `processes[].pid` | 프로세스 ID |
| `processes[].ppid` | 부모 프로세스 ID |
| `processes[].name` | 프로세스 이름 |
| `processes[].status` | 프로세스 상태 (running, sleeping, stopped 등) |
| `processes[].cpu_usage` | CPU 사용률 (퍼센트) |
| `processes[].memory_rss` | 실제 사용 중인 물리적 메모리 (RSS, Resident Set Size) (바이트) |
| `processes[].memory_vsz` | 가상 메모리 크기 (VSZ, Virtual Size) (바이트) |
| `processes[].threads` | 스레드 수 |
| `processes[].user` | 프로세스 소유자 사용자 |
| `processes[].command` | 실행 명령어 |
| `processes[].start_time` | 프로세스 시작 시간 (타임스탬프) |
| `processes[].cpu_time` | CPU 사용 시간 (초) |
| `processes[].io_read_bytes` | 읽은 바이트 수 |
| `processes[].io_write_bytes` | 쓴 바이트 수 |
| `processes[].open_files` | 열린 파일 수 |
| `processes[].nice` | 프로세스 우선순위 값 |

## Docker 컨테이너 정보
| 필드 | 설명 |
|------|------|
| `docker[].container_id` | 컨테이너 ID |
| `docker[].container_name` | 컨테이너 이름 |
| `docker[].container_image` | 컨테이너 이미지 |
| `docker[].container_status` | 컨테이너 상태 (running, exited 등) |
| `docker[].container_created` | 컨테이너 생성 시간 (타임스탬프) |
| `docker[].container_health.status` | 컨테이너 헬스 체크 상태 (healthy, unhealthy 등) |
| `docker[].container_health.failing_streak` | 헬스 체크 실패 횟수 |
| `docker[].container_health.last_check_output` | 마지막 헬스 체크 출력 |
| `docker[].container_ports[].container_port` | 컨테이너 내부 포트 |
| `docker[].container_ports[].host_port` | 호스트 포트 |
| `docker[].container_ports[].protocol` | 프로토콜 (tcp/udp) |
| `docker[].container_network[].network_name` | 네트워크 이름 |
| `docker[].container_network[].network_ip` | 네트워크 IP 주소 |
| `docker[].container_network[].network_mac` | 네트워크 MAC 주소 |
| `docker[].container_network[].network_rx_bytes` | 받은 바이트 수 |
| `docker[].container_network[].network_tx_bytes` | 보낸 바이트 수 |
| `docker[].container_volumes[].source` | 볼륨 소스 경로 (호스트 경로) |
| `docker[].container_volumes[].destination` | 볼륨 대상 경로 (컨테이너 경로) |
| `docker[].container_volumes[].mode` | 볼륨 모드 (ro: read-only, rw: read-write) |
| `docker[].container_volumes[].type` | 볼륨 유형 (bind: 바인드 마운트, volume: 도커 볼륨) |
| `docker[].container_env[].key` | 환경 변수 키 |
| `docker[].container_env[].value` | 환경 변수 값 |
| `docker[].cpu_usage` | CPU 사용률 (퍼센트) |
| `docker[].memory_usage` | 메모리 사용량 (바이트) |
| `docker[].memory_limit` | 메모리 한도 (바이트) |
| `docker[].memory_percent` | 메모리 사용률 (퍼센트) |
| `docker[].command` | 컨테이너 실행 명령어 |
| `docker[].network_rx_bytes` | 받은 총 바이트 수 |
| `docker[].network_tx_bytes` | 보낸 총 바이트 수 |
| `docker[].block_read` | 블록 장치에서 읽은 바이트 수 |
| `docker[].block_write` | 블록 장치에 쓴 바이트 수 |
| `docker[].pids` | 컨테이너 내 프로세스 ID 수 |
| `docker[].restarts` | 컨테이너 재시작 횟수 |
| `docker[].labels[].label_key` | 라벨 키 |
| `docker[].labels[].label_value` | 라벨 값 |

## 시스템 서비스 정보
| 필드 | 설명 |
|------|------|
| `services[].name` | 서비스 이름 |
| `services[].status` | 서비스 상태 (running, stopped, failed 등) |
| `services[].enabled` | 부팅 시 자동 시작 여부 (true/false) |
| `services[].type` | 서비스 유형 |
| `services[].load_state` | 서비스 로드 상태 |
| `services[].active_state` | 서비스 활성 상태 |
| `services[].sub_state` | 서비스 하위 상태 |
| `services[].memory_usage` | 서비스 메모리 사용량 (바이트) |
| `services[].cpu_usage` | 서비스 CPU 사용률 (퍼센트) |
