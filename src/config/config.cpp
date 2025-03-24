#include "config/config.h"
#include "log/logger.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <openssl/sha.h>
#include <sys/stat.h>

using namespace std;

Config::Config()
{
    generateSystemKey();
}

Config::~Config()
{
}

string Config::getSystemKey()
{
    return systemKey;
}

void Config::generateSystemKey()
{
    LOG_INFO("시스템 키 생성 시작");
    string keyPath = "/opt/system-monitor/system-monitor.key";

    // 디렉토리 존재 여부 확인 및 생성
    string dirPath = "/opt/system-monitor";
    if (access(dirPath.c_str(), F_OK) != 0)
    {
        if (mkdir(dirPath.c_str(), 0755) != 0)
        {
            LOG_ERROR("시스템 키 디렉토리 생성 실패: {}", dirPath);
            throw runtime_error("Failed to create directory: " + dirPath);
        }
    }

    // 기존 키 파일이 있는지 확인
    ifstream keyFile(keyPath);
    if (keyFile.good())
    {
        LOG_INFO("기존 시스템 키 파일 존재");
        string existingKey;
        getline(keyFile, existingKey);
        if (!existingKey.empty())
        {
            LOG_INFO("기존 시스템 키 파일 읽기 성공");
            systemKey = existingKey;
            return;
        }
    }

    LOG_INFO("새로운 시스템 키 생성 시작");
    // 새로운 키 생성
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 255);

    // 시스템 정보 수집
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    // 키 생성을 위한 데이터 조합
    stringstream ss;
    ss << hostname << "_";

    // 랜덤 값 추가
    for (int i = 0; i < 16; i++)
    {
        ss << hex << setw(2) << setfill('0') << dis(gen);
    }

    string newKey = ss.str();

    LOG_INFO("새로운 시스템 키 해시 생성 시작");
    // sha512 해시 생성
    unsigned char hash[SHA512_DIGEST_LENGTH];
    SHA512_CTX sha512;
    SHA512_Init(&sha512);
    SHA512_Update(&sha512, newKey.c_str(), newKey.size());
    SHA512_Final(hash, &sha512);

    // 해시값을 16진수 문자열로 변환
    stringstream hashStream;
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        hashStream << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    string hashedKey = hashStream.str();

    LOG_INFO("해시된 시스템 키 저장 시작");
    // 해시된 키 파일 저장
    ofstream outFile(keyPath);
    if (outFile.good())
    {
        outFile << hashedKey;
    }
    else
    {
        LOG_ERROR("시스템 키 파일 저장 실패: {}", keyPath);
        throw runtime_error("Failed to save system key: " + keyPath);
    }

    LOG_INFO("시스템 키 저장 완료");

    systemKey = hashedKey;
}
