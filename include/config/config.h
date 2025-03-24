#pragma once
#include <string>

class Config
{
private:
    std::string systemKey;
    void generateSystemKey();

public:
    Config();
    ~Config();
    std::string getSystemKey();
};