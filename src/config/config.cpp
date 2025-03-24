#include "config/config.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <openssl/sha.h>

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
    const char *homeDir = getenv("HOME");
    string keyPath = string(homeDir) + "/.system-monitor.key";

    // 기존 키 파일이 있는지 확인
    ifstream keyFile(keyPath);
    if (keyFile.good())
    {
        string existingKey;
        getline(keyFile, existingKey);
        if (!existingKey.empty())
        {
            systemKey = existingKey;
            return;
        }
    }

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

    // 해시된 키 파일 저장
    ofstream outFile(keyPath);
    if (outFile.good())
    {
        outFile << hashedKey;
    }

    systemKey = hashedKey;
}
