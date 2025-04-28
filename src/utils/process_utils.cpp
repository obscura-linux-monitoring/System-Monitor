#include "utils/process_utils.h"
#include <unistd.h>

using namespace std;

bool ProcessUtils::isProcessRunning(int pid)
{
    string checkCmd = "ps -p " + to_string(pid) + " >/dev/null 2>&1";
    return system(checkCmd.c_str()) == 0;
}

void ProcessUtils::waitForProcessToEnd(int pid, int maxWaitSeconds)
{
    int waited = 0;
    while (isProcessRunning(pid) && waited < maxWaitSeconds) {
        sleep(1);
        waited++;
    }
}

string ProcessUtils::escapeShellCommand(const string& cmd)
{
    string result = cmd;
    // 셸 특수 문자에 백슬래시 추가
    for (char c : "`|&;()<>$\\\"' \t\n") {
        string search(1, c);
        string replace = "\\" + search;
        size_t pos = 0;
        while ((pos = result.find(search, pos)) != string::npos) {
             result.replace(pos, 1, replace);
             pos += replace.length();
        }
    }
    return result;
}