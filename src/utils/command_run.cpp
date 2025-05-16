#include "utils/command_run.h"

using namespace std;

string exec(const char *command)
{
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);

    if (!pipe)
    {
        throw runtime_error("Failed to run command: " + string(command));
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }

    return result;
}