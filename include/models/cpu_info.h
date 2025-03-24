#pragma once

#include <string>
#include <vector>

using namespace std;

struct CpuCoreInfo
{
    int id;
    float usage;
    float temperature;
};

struct CpuInfo
{
    string model;
    string vendor;
    string architecture;
    float usage;
    float temperature;
    size_t total_cores;
    size_t total_logical_cores;
    bool is_hyperthreading;
    float clock_speed;
    int cache_size;
    vector<CpuCoreInfo> cores;
    bool has_vmx;             // Intel VT 지원 여부
    bool has_svm;             // AMD-V 지원 여부
    bool has_avx;             // AVX 지원 여부
    bool has_avx2;            // AVX2 지원 여부
    bool has_neon;            // NEON 지원 여부
    bool has_sve;             // SVE 지원 여부
};