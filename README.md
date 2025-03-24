# System Monitor (시스템 모니터)

시스템 리소스 모니터링 도구입니다. CPU, 메모리, 디스크, 네트워크 사용량을 실시간으로 모니터링할 수 있습니다.

## 주요 기능

- CPU 사용량 및 온도 모니터링
- 메모리 사용량 모니터링
- 디스크 사용량 모니터링
- 네트워크 트래픽 모니터링
- ncurses 기반 TUI (Text User Interface) 지원
- 서버-클라이언트 모드 지원

## 시스템 요구사항

- C++17 이상
- CMake 3.10 이상
- 필수 라이브러리:
  - libstatgrab
  - libsensors
  - ncurses
  - websocketpp
  - spdlog

## 설치 방법

```shell
# 설치 명령어들이 들어갈 자리
```

## 사용 방법

```shell
./system_monitor [옵션]

옵션:
  -n, --ncurses        ncurses 모드 사용
  -s, --server         서버 URL (예: @http://127.0.0.1:80)
  -c, --collection     수집 간격 (초, 기본값: 5)
  -t, --transmission   전송 간격 (초, 기본값: 5)
  -h, --help          도움말 표시
```

## 프로젝트 구조
```
src/
├── collectors/ # 시스템 정보 수집 모듈
├── ui/ # 사용자 인터페이스 관련 코드
├── network/ # 네트워크 통신 관련 코드
├── log/ # 로깅 시스템
└── models/ # 데이터 모델
```

## 수집 데이터 구조

시스템 모니터가 수집하는 데이터의 구조는 다음 문서들을 참조하세요:

- [데이터 구조 상세 설명](./docs/data_struct.md)
- [JSON 스키마 예시](./docs/data_struct.json)

## 개발 환경 설정

개발 환경 설정 방법 설명...

## 라이선스

라이선스 정보...

## 기여하기

프로젝트 기여 방법...

## 문제 해결

자주 발생하는 문제와 해결 방법...

## 연락처

연락처 정보...