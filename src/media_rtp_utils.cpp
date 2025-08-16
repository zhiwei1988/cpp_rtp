#include "media_rtp_utils.h"
#include "rtperrors.h"
#include <random>
#include <mutex>
#include <thread>
#include <chrono>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>

// 随机数生成相关
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

// RTPTime 实现
RTPTime::RTPTime(double t) : m_seconds(t) {}

RTPTime::RTPTime(int64_t seconds, uint32_t microseconds) {
    if (seconds >= 0) {
        m_seconds = static_cast<double>(seconds) + 1e-6 * static_cast<double>(microseconds);
    } else {
        int64_t possec = -seconds;
        m_seconds = static_cast<double>(possec) + 1e-6 * static_cast<double>(microseconds);
        m_seconds = -m_seconds;
    }
}

RTPTime::RTPTime(RTPNTPTime ntptime) {
    if (ntptime.GetMSW() < RTP_NTPTIMEOFFSET) {
        m_seconds = 0;
    } else {
        uint32_t sec = ntptime.GetMSW() - RTP_NTPTIMEOFFSET;
        
        double x = static_cast<double>(ntptime.GetLSW());
        x /= (65536.0 * 65536.0);
        x *= 1000000.0;
        uint32_t microsec = static_cast<uint32_t>(x);

        m_seconds = static_cast<double>(sec) + 1e-6 * static_cast<double>(microsec);
    }
}

int64_t RTPTime::GetSeconds() const {
    return static_cast<int64_t>(m_seconds);
}

uint32_t RTPTime::GetMicroSeconds() const {
    uint32_t microsec;

    if (m_seconds >= 0) {
        int64_t sec = static_cast<int64_t>(m_seconds);
        microsec = static_cast<uint32_t>(1e6 * (m_seconds - static_cast<double>(sec)) + 0.5);
    } else {
        int64_t sec = static_cast<int64_t>(-m_seconds);
        microsec = static_cast<uint32_t>(1e6 * ((-m_seconds) - static_cast<double>(sec)) + 0.5);
    }

    if (microsec >= 1000000)
        return 999999;
    return microsec;
}

RTPNTPTime RTPTime::GetNTPTime() const {
    uint32_t sec = static_cast<uint32_t>(m_seconds);
    uint32_t microsec = static_cast<uint32_t>((m_seconds - static_cast<double>(sec)) * 1e6);

    uint32_t msw = sec + RTP_NTPTIMEOFFSET;
    uint32_t lsw;
    double x;
    
    x = microsec / 1000000.0;
    x *= (65536.0 * 65536.0);
    lsw = static_cast<uint32_t>(x);

    return RTPNTPTime(msw, lsw);
}

RTPTime RTPTime::CurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration - seconds);
    
    return RTPTime(seconds.count(), static_cast<uint32_t>(microseconds.count()));
}

void RTPTime::Wait(const RTPTime &delay) {
    if (delay.m_seconds <= 0)
        return;

    auto duration = std::chrono::duration<double>(delay.m_seconds);
    std::this_thread::sleep_for(duration);
}

RTPTime &RTPTime::operator-=(const RTPTime &t) { 
    m_seconds -= t.m_seconds;
    return *this;
}

RTPTime &RTPTime::operator+=(const RTPTime &t) { 
    m_seconds += t.m_seconds;
    return *this;
}

bool RTPTime::operator<(const RTPTime &t) const {
    return m_seconds < t.m_seconds;
}

bool RTPTime::operator>(const RTPTime &t) const {
    return m_seconds > t.m_seconds;
}

bool RTPTime::operator<=(const RTPTime &t) const {
    return m_seconds <= t.m_seconds;
}

bool RTPTime::operator>=(const RTPTime &t) const {
    return m_seconds >= t.m_seconds;
}

// RTPSelect 实现 (基于select，不使用poll)
int RTPSelect(const int *sockets, int8_t *readflags, size_t numsocks, RTPTime timeout) {
    struct timeval tv;
    struct timeval *pTv = nullptr;

    if (timeout.GetDouble() >= 0) {
        tv.tv_sec = static_cast<long>(timeout.GetSeconds());
        tv.tv_usec = timeout.GetMicroSeconds();
        pTv = &tv;
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    for (size_t i = 0; i < numsocks; i++) {
        const int setsize = FD_SETSIZE;
        if (sockets[i] >= setsize)
            return MEDIA_RTP_ERR_OPERATION_FAILED;
        FD_SET(sockets[i], &fdset);
        readflags[i] = 0;
    }

    int status = select(FD_SETSIZE, &fdset, nullptr, nullptr, pTv);
    if (status < 0) {
        // 忽略 EINTR 中断
        if (errno == EINTR)
            return 0;
        return MEDIA_RTP_ERR_OPERATION_FAILED;
    }

    if (status > 0) {
        for (size_t i = 0; i < numsocks; i++) {
            if (FD_ISSET(sockets[i], &fdset))
                readflags[i] = 1;
        }
    }
    return status;
}