#!/bin/bash

# last modified: 2025-03-24 16:25

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
DOWNLOAD_EXCUABLE_URL="https://github.com/obscura-linux-monitoring/System-Monitor/releases/download/dev/client.exec"
DOWNLOAD_SERVICE_URL="https://github.com/obscura-linux-monitoring/System-Monitor/releases/download/dev/system-monitor.service"
DOWNLOAD_UNINSTALL_SCRIPT_URL="https://github.com/obscura-linux-monitoring/System-Monitor/releases/download/dev/uninstall.sh"


# 배포판 확인
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    VERSION=$VERSION_ID
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
            apt-get install -y curl libcurl4-openssl-dev libssl-dev libspdlog-dev \
                              nlohmann-json3-dev libstatgrab-dev libsensors-dev \
                              libncursesw5-dev libprocps-dev libjsoncpp-dev libsystemd-dev || return 1
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

# 실행 파일 다운로드
download_executable() {
    echo "실행 파일 다운로드 중..."
    wget -O $BIN_DIR/client.exec $DOWNLOAD_EXCUABLE_URL || return 1
    chmod +x $BIN_DIR/client.exec || return 1
    echo "실행 파일 다운로드 완료"
    return 0
}

# 서비스 파일 다운로드
download_service_file() {
    echo "서비스 파일 다운로드 중..."
    wget -O /etc/systemd/system/$SERVICE_NAME.service $DOWNLOAD_SERVICE_URL || return 1
    echo "서비스 파일 다운로드 완료"
    return 0
}

# 언인스톨 스크립트 다운로드
download_uninstall_script() {
    echo "언인스톨 스크립트 다운로드 중..."
    wget -O $INSTALL_DIR/$UNINSTALL_SCRIPT $DOWNLOAD_UNINSTALL_SCRIPT_URL || return 1
    chmod +x $INSTALL_DIR/$UNINSTALL_SCRIPT || return 1
    echo "언인스톨 스크립트 다운로드 완료"
    return 0
}

# 메인 설치 과정
echo "시스템 모니터 설치 시작..."

create_directories
if [ $? -ne 0 ]; then
    echo "디렉터리 생성 실패. 설치를 중단합니다."
    exit 1
fi

install_dependencies
if [ $? -ne 0 ]; then
    echo "의존성 패키지 설치 실패. 설치를 중단합니다."
    exit 1
fi

download_executable
if [ $? -ne 0 ]; then
    echo "실행 파일 다운로드 실패. 설치를 중단합니다."
    exit 1
fi

download_uninstall_script
if [ $? -ne 0 ]; then
    echo "언인스톨 스크립트 다운로드 실패. 설치를 중단합니다."
    exit 1
fi

download_service_file
if [ $? -ne 0 ]; then
    echo "서비스 파일 다운로드 실패. 설치를 중단합니다."
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