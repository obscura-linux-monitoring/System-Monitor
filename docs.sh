# start_docs.sh 생성
#!/bin/bash

# 빌드 디렉토리 생성 및 이동
mkdir -p build
cd build

# CMake 실행 및 문서 생성
cmake ..
make docs

# 이전에 실행 중인 문서 서버 종료
pid=$(lsof -t -i:7878)
if [ ! -z "$pid" ]; then
    kill $pid
fi

# 백그라운드에서 문서 서버 실행
cd ../docs/html
python3 -m http.server 7878 > /dev/null 2>&1 &

echo "문서 서버가 시작되었습니다."
echo "VS Code에서 포트 7878 포워딩하여 접속하세요."
echo "또는 http://localhost:7878 으로 접속하세요."