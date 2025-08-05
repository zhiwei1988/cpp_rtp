/**
 * \file rtp_protocol_utils.h
 * 
 * 统一的RTP协议工具模块，包含时间处理、socket操作和网络选择功能
 */

#ifndef RTP_PROTOCOL_UTILS_H
#define RTP_PROTOCOL_UTILS_H

#include "rtpconfig.h"
#include <cstdint>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#ifdef RTP_HAVE_SYS_FILIO
    #include <sys/filio.h>
#endif
#ifdef RTP_HAVE_SYS_SOCKIO
    #include <sys/sockio.h>
#endif
#ifdef RTP_SUPPORT_IFADDRS
    #include <ifaddrs.h>
#endif

// Socket 工具宏定义
#define RTPSOCKERR                              -1
#define RTPCLOSE(x)                             close(x)

#ifdef RTP_SOCKLENTYPE_UINT
    #define RTPSOCKLENTYPE                      unsigned int
#else
    #define RTPSOCKLENTYPE                      int
#endif

#define RTPIOCTL                                ioctl

// 时间相关常量
#define RTP_NTPTIMEOFFSET                       2208988800UL

/**
 * NTP时间戳包装类
 */
class RTPNTPTime
{
public:
    RTPNTPTime(uint32_t m, uint32_t l) : msw(m), lsw(l) {}
    
    uint32_t GetMSW() const { return msw; }
    uint32_t GetLSW() const { return lsw; }
    
private:
    uint32_t msw, lsw;
};

/**
 * 现代化的时间处理类，使用 std::chrono 作为底层实现
 */
class RTPTime
{
public:
    // 静态方法
    static RTPTime CurrentTime();
    static void Wait(const RTPTime &delay);
    
    // 构造函数
    RTPTime(double t = 0.0);
    RTPTime(RTPNTPTime ntptime);
    RTPTime(int64_t seconds, uint32_t microseconds);
    
    // 获取时间值
    int64_t GetSeconds() const;
    uint32_t GetMicroSeconds() const;
    double GetDouble() const { return m_seconds; }
    RTPNTPTime GetNTPTime() const;
    
    // 操作符重载
    RTPTime &operator-=(const RTPTime &t);
    RTPTime &operator+=(const RTPTime &t);
    bool operator<(const RTPTime &t) const;
    bool operator>(const RTPTime &t) const;
    bool operator<=(const RTPTime &t) const;
    bool operator>=(const RTPTime &t) const;
    
    bool IsZero() const { return m_seconds == 0.0; }
    
private:
    double m_seconds;  // 使用double存储秒数，保持兼容性
};

/**
 * 网络socket选择函数 (基于select实现)
 * 
 * @param sockets   要检查的socket数组
 * @param readflags 输出标志数组，如果对应socket有数据则设置为1
 * @param numsocks  socket数量
 * @param timeout   超时时间，负值表示无限等待
 * @return 有数据的socket数量，出错返回负值
 */
int RTPSelect(const int *sockets, int8_t *readflags, size_t numsocks, RTPTime timeout);

// 随机数生成函数 (保留原有功能)
uint8_t RTPGenerateRandom8();
uint16_t RTPGenerateRandom16();
uint32_t RTPGenerateRandom32();
double RTPGenerateRandomDouble();

#endif // RTP_PROTOCOL_UTILS_H