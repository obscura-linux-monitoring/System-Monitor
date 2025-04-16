FROM ubuntu:22.04

# 비대화형 설치 모드 설정 및 시간대 미리 구성
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Seoul

# 해시 불일치 문제 해결을 위한 APT 설정 추가
RUN echo 'Acquire::http::Pipeline-Depth "0";' > /etc/apt/apt.conf.d/99custom && \
    echo 'Acquire::CompressionTypes::Order:: "gz";' >> /etc/apt/apt.conf.d/99custom && \
    echo 'Acquire::http::No-Cache "true";' >> /etc/apt/apt.conf.d/99custom && \
    echo 'Acquire::BrokenProxy "true";' >> /etc/apt/apt.conf.d/99custom

# APT 설정 업데이트 및 패키지 설치
RUN apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    apt-get update -o Acquire::Max-FutureTime=86400 --fix-missing && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    libcurl4-openssl-dev \
    libsystemd-dev \
    libsensors4-dev \
    nlohmann-json3-dev \
    lm-sensors \
    procps \
    net-tools \
    iproute2 \
    tzdata \
    libssl-dev \
    libspdlog-dev \
    zlib1g-dev \
    libstatgrab-dev \
    libncursesw5-dev \
    libjsoncpp-dev \
    libprocps-dev \
    libwebsocketpp-dev \
    libboost-all-dev \
    && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# 작업 디렉토리 생성
WORKDIR /app

# 소스 코드 복사
COPY . .

# 빌드 디렉토리 생성 및 빌드 실행
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

COPY entrypoint.sh .
RUN chmod +x entrypoint.sh
ENTRYPOINT ["./entrypoint.sh"]