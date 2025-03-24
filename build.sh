#!/bin/bash

# CPU 코어 수 확인
CORES=$(nproc)

# bin 삭제
rm -f bin/*

# build 디렉토리로 이동
cd build

# CMake 캐시가 없을 경우에만 cmake 실행
if [ ! -f "CMakeCache.txt" ]; then
    cmake ..
fi

# 병렬 빌드 실행 (-j 옵션 사용)
make clean
make -j$CORES 