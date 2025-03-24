#!/bin/bash

# last modified: 2025-03-24 16:25

# 루트 권한 확인
if [ "$EUID" -ne 0 ]; then
  echo "루트 권한으로 실행해주세요 (sudo ./uninstall.sh)"
  exit 1
fi

# 변수 설정
INSTALL_DIR="/opt/system-monitor"
SERVICE_NAME="system-monitor"

# 서비스 중지 및 제거
if ! systemctl stop $SERVICE_NAME; then
    echo "서비스 중지 중 오류가 발생했습니다."
fi

if ! systemctl disable $SERVICE_NAME; then
    echo "서비스 비활성화 중 오류가 발생했습니다."
fi

if ! rm -f /etc/systemd/system/$SERVICE_NAME.service; then
    echo "서비스 파일 제거 중 오류가 발생했습니다."
fi

systemctl daemon-reload

# 설치 디렉터리 제거
if ! rm -rf $INSTALL_DIR; then
    echo "설치 디렉터리 제거 중 오류가 발생했습니다."
    exit 1
fi

echo "시스템 모니터가 성공적으로 제거되었습니다."