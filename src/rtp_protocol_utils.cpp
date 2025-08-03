#include "rtp_protocol_utils.h"
#include <random>
#include <mutex>

static std::mt19937& GetRandomGenerator() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

static std::mutex& GetRandomMutex() {
    static std::mutex mtx;
    return mtx;
}

uint8_t RTPGenerateRandom8() {
    std::lock_guard<std::mutex> lock(GetRandomMutex());
    std::uniform_int_distribution<uint32_t> dist(0, 255);
    return static_cast<uint8_t>(dist(GetRandomGenerator()));
}

uint16_t RTPGenerateRandom16() {
    std::lock_guard<std::mutex> lock(GetRandomMutex());
    std::uniform_int_distribution<uint32_t> dist(0, 65535);
    return static_cast<uint16_t>(dist(GetRandomGenerator()));
}

uint32_t RTPGenerateRandom32() {
    std::lock_guard<std::mutex> lock(GetRandomMutex());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    return dist(GetRandomGenerator());
}

double RTPGenerateRandomDouble() {
    std::lock_guard<std::mutex> lock(GetRandomMutex());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(GetRandomGenerator());
}