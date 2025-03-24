#!/bin/bash

# 루트 권한 확인
if [ "$EUID" -ne 0 ]; then
  echo "루트 권한으로 실행해주세요 (sudo ./uninstall.sh)"
  exit 1
fi

# 변수 설정
INSTALL_DIR="/opt/system-monitor"
SERVICE_NAME="system-monitor"

# 서비스 중지 및 제거
systemctl stop $SERVICE_NAME
systemctl disable $SERVICE_NAME
rm -f /etc/systemd/system/$SERVICE_NAME.service
systemctl daemon-reload

# 설치 디렉터리 제거
rm -rf $INSTALL_DIR

echo "시스템 모니터가 성공적으로 제거되었습니다."