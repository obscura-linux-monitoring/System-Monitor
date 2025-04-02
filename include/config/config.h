#pragma once
#include <string>

using namespace std;

/**
 * @brief 시스템 구성 관리 클래스
 *
 * 시스템 설정 및 구성 정보를 관리하는 클래스로,
 * 시스템 키 생성 및 접근 기능을 제공합니다.
 */
class Config
{
private:
    /** @brief 시스템 인증에 사용되는 고유 키 */
    string systemKey;

    /**
     * @brief 시스템 키를 생성하는 내부 메서드
     *
     * 고유한 시스템 키를 생성하여 systemKey 멤버에 저장합니다.
     */
    void generateSystemKey();

public:
    /**
     * @brief Config 클래스의 생성자
     *
     * 객체 생성 시 기본 설정값을 초기화하고 시스템 키를 생성합니다.
     */
    Config();

    /**
     * @brief Config 클래스의 소멸자
     *
     * 사용된 자원을 정리하고 객체를 안전하게 소멸시킵니다.
     */
    ~Config();

    /**
     * @brief 시스템 키를 반환하는 메서드
     *
     * @return 현재 설정된 시스템 키 문자열
     */
    string getSystemKey();
};