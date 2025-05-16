#pragma once

#include <array>
#include <string>
#include <cstdio>
#include <memory>

using namespace std;

/**
 * @brief 주어진 명령어를 실행하고 결과를 반환합니다.
 *
 * 주어진 명령어를 실행하고 결과를 반환합니다.
 *
 * @param command 실행할 명령어
 * @return 명령어 실행 결과
 */
string exec(const char *command);