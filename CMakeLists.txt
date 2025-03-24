cmake_minimum_required(VERSION 3.10)

# 프로젝트 이름 변수화
set(PROJECT_NAME system_monitor)
project(${PROJECT_NAME} VERSION 1.0)

set(EXECUTABLE_NAME client)

# C++17 표준 설정
set(CMAKE_CXX_STANDARD 17)

# 빌드 결과물을 bin 폴더에 저장하도록 설정
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 실행 파일 확장자를 .exe로 설정
set(CMAKE_EXECUTABLE_SUFFIX ".exe")

# 필요한 모든 패키지 찾기 - 먼저 검색하여 효율성 향상
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

# libstatgrab 라이브러리
pkg_check_modules(STATGRAB REQUIRED libstatgrab)

# sensors 라이브러리
find_library(SENSORS_LIBRARY NAMES sensors)
if(NOT SENSORS_LIBRARY)
    message(FATAL_ERROR "sensors library not found")
endif()

# ncurses 라이브러리
pkg_check_modules(NCURSESW REQUIRED ncursesw)

# procps 라이브러리
pkg_check_modules(PROCPS REQUIRED libprocps)

# jsoncpp 라이브러리
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# libsystemd 개발 패키지
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

# 소스 파일들을 각 디렉토리별로 추가 (GLOB_RECURSE 대신 명시적으로 설정하는 것이 권장됨)
file(GLOB_RECURSE SOURCES 
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/collectors/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/models/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/utils/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/network/${EXECUTABLE_NAME}/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/network/common/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/log/*.cpp"
)

# 실행 파일 생성 - 이름을 ${EXECUTABLE_NAME}로 변경
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# 컴파일러 경고 플래그 설정 - 타겟에 직접 적용
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${EXECUTABLE_NAME} PRIVATE
        -Wall              # 기본적인 모든 경고
        -Wextra           # 추가 경고
        -Wpedantic        # 표준 준수 검사
        -Wshadow          # 변수 섀도잉 감지
        -Wformat=2        # 문자열 포맷 검사 강화
        -Wunused          # 사용되지 않는 변수/함수 감지
        -Wcast-align      # 메모리 정렬 관련 경고
        -Wconversion      # 암시적 변환 경고
        -Wsign-conversion # 부호 변환 경고
        -Wnull-dereference # null 포인터 참조 검사
    )
endif()

# filesystem 라이브러리 연결 - 타겟에 직접 적용
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE stdc++fs)
endif()

# websocketpp 라이브러리 경로 설정
set(WEBSOCKETPP_INCLUDE_DIR "/usr/include")

# 헤더 파일 경로 - 타겟에 직접 적용
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${JSONCPP_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${SYSTEMD_INCLUDE_DIRS}
    ${WEBSOCKETPP_INCLUDE_DIR}
)

# 모든 라이브러리를 한 번에 링크
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    ${STATGRAB_LIBRARIES}
    Threads::Threads
    ${SENSORS_LIBRARY}
    ${NCURSESW_LIBRARIES}
    CURL::libcurl
    ${PROCPS_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    OpenSSL::Crypto
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIBRARIES}
)