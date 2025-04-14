#!/bin/bash

# 루트 권한 확인
if [ "$EUID" -ne 0 ]; then
  echo "루트 권한으로 실행해주세요 (sudo ./install.sh)"
  exit 1
fi

# 변수 설정
INSTALL_DIR="/opt/system-monitor"
BIN_DIR="$INSTALL_DIR/bin"
SERVICE_NAME="system-monitor"
UNINSTALL_SCRIPT="uninstall.sh"
SERVER_URL="1.209.148.143:8087"

# 버전 매개변수 확인
VERSION=${1:-"dev"}  # 매개변수가 없으면 기본값 "dev" 사용

# KEY 매개변수 확인
if [ -z "$2" ]; then
  echo "USER_ID 매개변수가 필요합니다 (sudo ./install.sh VERSION USER_ID)"
  exit 1
fi
USER_ID=${2}

BASE_URL="https://github.com/obscura-linux-monitoring/System-Monitor/releases/download/$VERSION"
DOWNLOAD_EXCUABLE_URL="$BASE_URL/client.exec"
DOWNLOAD_SERVICE_URL="$BASE_URL/system-monitor.service"
DOWNLOAD_UNINSTALL_SCRIPT_URL="$BASE_URL/uninstall.sh"

# 배포판 확인
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    VERSION_ID=$(echo $VERSION_ID | tr -d '"')  # 따옴표 제거
    
    # Ubuntu 20.04 버전 체크
    if [ "$OS" != "ubuntu" ] || [ "$VERSION_ID" != "20.04" ]; then
        echo "이 프로그램은 Ubuntu 20.04에서만 실행 가능합니다."
        exit 1
    fi
else
    echo "지원되지 않는 리눅스 배포판입니다."
    exit 1
fi

# 의존성 패키지 설치
install_dependencies() {
    echo "[$OS] 의존성 패키지 설치 중..."
    
    case $OS in
        ubuntu|debian)
            apt-get update || return 1
            
            # Ubuntu 버전별 패키지 이름 설정
            case $VERSION_ID in
                "22.04")  # Ubuntu Jammy
                    apt-get install -y \
                        libsensors5 \
                        libncursesw6 \
                        libcurl4 \
                        libprocps8 \
                        libssl3 \
                        libspdlog1 \
                        libsystemd0 \
                        libstdc++6 || return 1
                    ;;
                "24.04")  # Ubuntu Noble
                    apt-get install -y \
                        libsensors5 \
                        libncursesw6 \
                        libcurl4 \
                        libproc2-0 \
                        libssl3t64 \
                        libspdlog1.12 \
                        libsystemd0 \
                        libstdc++6 || return 1
                    ;;
                "20.04")  # Ubuntu Focal
                    apt-get install -y \
                        libsensors5 \
                        libncursesw6 \
                        libcurl4 \
                        libprocps8 \
                        libssl1.1 \
                        libspdlog1 \
                        libsystemd0 \
                        libstdc++6 || return 1
                    ;;
                *)
                    echo "지원되지 않는 Ubuntu 버전: $VERSION_ID"
                    return 1
                    ;;
            esac
            ;;
        centos|rhel|rocky)
            if [ "$OS" = "centos" ] && [ "$VERSION_ID" -lt 8 ]; then
                yum install -y epel-release || return 1
                yum install -y curl libcurl-devel openssl-devel libstatgrab-devel \
                              lm_sensors-devel ncurses-devel procps-ng-devel jsoncpp-devel \
                              systemd-devel || return 1
            else
                # CentOS 8+, Rocky, RHEL 8+
                dnf install -y epel-release || return 1
                dnf install -y curl libcurl-devel openssl-devel spdlog-devel \
                             nlohmann-json-devel libstatgrab-devel lm_sensors-devel \
                             ncurses-devel procps-ng-devel jsoncpp-devel systemd-devel || return 1
            fi
            ;;
        *)
            echo "지원되지 않는 배포판: $OS"
            return 1
            ;;
    esac
    
    echo "의존성 패키지 설치 완료"
    return 0
}

# 디렉터리 생성
create_directories() {
    echo "디렉터리 검사 중..."
    if [ -d "$INSTALL_DIR" ]; then
        echo "오류: $INSTALL_DIR 가 이미 존재합니다."
        echo "이미 설치되어 있을 수 있습니다. 제거 후 다시 시도해주세요."
        return 1
    fi
    
    echo "디렉터리 생성 중..."
    mkdir -p $BIN_DIR || return 1
    echo "디렉터리 생성 완료"
    return 0
}

# 기존 download_executable, download_service_file, download_uninstall_script 함수 대신 
# 아래 단일 함수로 대체

download_files() {
    echo "실행 파일 다운로드 중..."
    if ! wget -O $BIN_DIR/client.exec $DOWNLOAD_EXCUABLE_URL; then
        echo "실행 파일 다운로드 실패"
        return 1
    fi
    chmod +x $BIN_DIR/client.exec

    echo "서비스 파일 다운로드 중..."
    if ! wget -O /etc/systemd/system/$SERVICE_NAME.service $DOWNLOAD_SERVICE_URL; then
        echo "서비스 파일 다운로드 실패"
        return 1
    fi
    
    # 서비스 파일 내용 수정
    echo "서비스 파일 설정 업데이트 중..."
    sed -i "s|\[SERVER_URL\]|$SERVER_URL|g" /etc/systemd/system/$SERVICE_NAME.service
    sed -i "s|\[USER_ID\]|$USER_ID|g" /etc/systemd/system/$SERVICE_NAME.service
    
    echo "언인스톨 스크립트 다운로드 중..."
    if ! wget -O $INSTALL_DIR/$UNINSTALL_SCRIPT $DOWNLOAD_UNINSTALL_SCRIPT_URL; then
        echo "언인스톨 스크립트 다운로드 실패"
        return 1
    fi
    chmod +x $INSTALL_DIR/$UNINSTALL_SCRIPT
    
    echo "파일 다운로드 완료"
    return 0
}

# 정리 함수 추가
cleanup() {
    echo "설치 파일 정리 중..."
    
    # 서비스 파일 제거
    rm -f /etc/systemd/system/$SERVICE_NAME.service 2>/dev/null
    
    # 설치 디렉터리 제거
    rm -rf $INSTALL_DIR 2>/dev/null
    
    echo "정리 완료"
}

# 메인 설치 과정
echo "시스템 모니터 설치 시작..."

create_directories
if [ $? -ne 0 ]; then
    echo "디렉터리 생성 실패. 설치를 중단합니다."
    cleanup
    exit 1
fi

# install_dependencies
# if [ $? -ne 0 ]; then
#     echo "의존성 패키지 설치 실패. 설치를 중단합니다."
#     cleanup
#     exit 1
# fi

download_files
if [ $? -ne 0 ]; then
    echo "파일 다운로드 실패. 설치를 중단합니다."
    cleanup
    exit 1
fi

start_and_enable_service() {
    echo "서비스 시작 중..."
    sudo systemctl start $SERVICE_NAME
    echo "서비스 시작 완료"
    echo "서비스 활성화 중..."
    sudo systemctl enable $SERVICE_NAME
    echo "서비스 활성화 완료"
    return 0
}

start_and_enable_service
if [ $? -ne 0 ]; then
    echo "서비스 시작 및 활성화 실패. 수동으로 시작 및 활성화 해주세요."
    exit 1
fi

echo "설치가 완료되었습니다!"
echo "상태 확인: sudo systemctl status $SERVICE_NAME"