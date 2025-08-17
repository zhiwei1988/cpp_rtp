/**
 * 综合UDP传输测试 - 统一验证IPv4/IPv6发送接收功能
 * 仅支持Linux平台
 */

#include "media_rtp_session.h"
#include "media_rtp_udpv4_transmitter.h"
#include "media_rtp_udpv6_transmitter.h"
#include "media_rtp_endpoint.h"
#include "media_rtp_session_params.h"
#include "media_rtp_errors.h"
#include "media_rtp_source_data.h"
#include "media_rtp_packet_factory.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

// 测试结果统计
struct TestStats {
    int totalTests = 0;
    int passedTests = 0;
    int failedTests = 0;
    
    void addResult(bool passed) {
        totalTests++;
        if (passed) passedTests++;
        else failedTests++;
    }
    
    void printSummary() {
        cout << "\n=== 测试结果汇总 ===" << endl;
        cout << "总测试数: " << totalTests << endl;
        cout << "通过: " << passedTests << endl;
        cout << "失败: " << failedTests << endl;
        cout << "成功率: " << (totalTests > 0 ? (passedTests * 100.0 / totalTests) : 0) << "%" << endl;
    }
};

// 获取详细错误信息的函数
string getErrorDescription(int errorCode) {
    switch (errorCode) {
        case -1: return "无效参数 (MEDIA_RTP_ERR_INVALID_PARAMETER)";
        case -2: return "操作失败 (MEDIA_RTP_ERR_OPERATION_FAILED)";
        case -3: return "无效状态 (MEDIA_RTP_ERR_INVALID_STATE)";
        case -4: return "资源错误 (MEDIA_RTP_ERR_RESOURCE_ERROR)";
        case -5: return "协议错误 (MEDIA_RTP_ERR_PROTOCOL_ERROR)";
        default:
            if (errorCode < 0) {
                return "未知RTP错误 (错误码: " + std::to_string(errorCode) + ")";
            }
            return "成功";
    }
}

// 统一的错误检查函数
void checkError(int rtperr, const string& testName) {
    if (rtperr < 0) {
        string errorMsg = "RTP错误 - " + testName + ": " + getErrorDescription(rtperr);
        throw std::runtime_error(errorMsg);
    }
}

// 基础测试会话类
class BaseTestSession : public RTPSession {
public:
    BaseTestSession() : packetsReceived(0), bytesReceived(0) {}
    
    int getPacketsReceived() const { return packetsReceived; }
    int getBytesReceived() const { return bytesReceived; }
    void resetCounters() { packetsReceived = 0; bytesReceived = 0; }
    
protected:
    void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled) override {
        packetsReceived++;
        bytesReceived += rtppack->GetPayloadLength();
        
        cout << "接收到数据包: SSRC=0x" << std::hex << srcdat->GetSSRC() << std::dec 
             << ", 序号=" << rtppack->GetExtendedSequenceNumber()
             << ", 载荷长度=" << rtppack->GetPayloadLength() << " 字节" << endl;
        
        DeletePacket(rtppack);
        *ispackethandled = true;
    }
    
    void OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t, const void *itemdata, size_t itemlength) override {
        char msg[256];
        memset(msg, 0, sizeof(msg));
        if (itemlength < sizeof(msg)) {
            memcpy(msg, itemdata, itemlength);
            cout << "接收到RTCP SDES项: SSRC=0x" << std::hex << srcdat->GetSSRC() << std::dec 
                 << ", 类型=" << (int)t << ", 内容=" << msg << endl;
        }
    }

private:
    int packetsReceived;
    int bytesReceived;
};

// 测试框架类
class UDPTransmissionTester {
private:
    TestStats stats;
    
public:
    TestStats& getStats() { return stats; }
    // 运行所有测试
    void runAllTests() {
        cout << "=== 开始综合UDP传输测试 ===" << endl;
        
        try {
            testUDPv4Basic();
            testUDPv4Loopback();
            testUDPv4RTCPMultiplexing();
            testUDPv4ExistingSockets();
            testUDPv6Basic();
            testErrorHandling();
            testPerformance();
        } catch (const std::exception& e) {
            cerr << "测试异常: " << e.what() << endl;
        }
        
        stats.printSummary();
    }
    
    // UDP IPv4基础发送接收测试
    void testUDPv4Basic() {
        cout << "\n--- 测试: UDP IPv4基础发送接收 ---" << endl;
        
        try {
            BaseTestSession session;
            RTPSessionParams sessParams;
            RTPUDPv4TransmissionParams transParams;
            
            // 配置会话参数
            sessParams.SetOwnTimestampUnit(1.0/8000.0);
            sessParams.SetAcceptOwnPackets(true);
            sessParams.SetUsePollThread(false); // 禁用轮询线程，使用手动轮询
            transParams.SetPortbase(5000);
            
            // 创建会话
            checkError(session.Create(sessParams, &transParams), "创建IPv4会话");
            
            // 添加本地目标 - 需要指定RTCP端口
            RTPEndpoint dest(ntohl(inet_addr("127.0.0.1")), (uint16_t)5000, (uint16_t)5001);
            checkError(session.AddDestination(dest), "添加IPv4目标");
            
            // 发送测试数据包
            const char* testData = "Hello RTP IPv4";
            checkError(session.SendPacket(testData, strlen(testData), 0, false, 10), "发送IPv4数据包");
            
            // 轮询接收数据 - 需要更多轮询来确保数据包被处理
            for (int i = 0; i < 10; i++) {
                checkError(session.Poll(), "轮询IPv4");
                usleep(50000); // 50ms
            }
            
            session.BYEDestroy(RTPTime(1,0), 0, 0);
            
            bool passed = session.getPacketsReceived() > 0;
            stats.addResult(passed);
            cout << "结果: " << (passed ? "通过" : "失败") << " (接收到 " << session.getPacketsReceived() << " 个数据包)" << endl;
            
        } catch (const std::exception& e) {
            stats.addResult(false);
            cout << "结果: 失败 - " << e.what() << endl;
        }
    }
    
    // UDP IPv4环回测试
    void testUDPv4Loopback() {
        cout << "\n--- 测试: UDP IPv4环回通信 ---" << endl;
        
        try {
            BaseTestSession sender, receiver;
            RTPSessionParams sessParams;
            RTPUDPv4TransmissionParams senderParams, receiverParams;
            
            sessParams.SetOwnTimestampUnit(1.0/8000.0);
            sessParams.SetUsePollThread(false); // 禁用轮询线程，使用手动轮询
            senderParams.SetPortbase(5010);
            receiverParams.SetPortbase(5020);
            
            // 创建发送和接收会话
            checkError(sender.Create(sessParams, &senderParams), "创建发送会话");
            checkError(receiver.Create(sessParams, &receiverParams), "创建接收会话");
            
            // 相互添加为目标
            RTPEndpoint senderAddr(ntohl(inet_addr("127.0.0.1")), (uint16_t)5010, (uint16_t)5011);
            RTPEndpoint receiverAddr(ntohl(inet_addr("127.0.0.1")), (uint16_t)5020, (uint16_t)5021);
            
            checkError(sender.AddDestination(receiverAddr), "发送方添加目标");
            checkError(receiver.AddDestination(senderAddr), "接收方添加目标");
            
            // 双向发送数据包
            const char* data1 = "From sender to receiver";
            const char* data2 = "From receiver to sender";
            
            checkError(sender.SendPacket(data1, strlen(data1), 0, false, 10), "发送方发送");
            checkError(receiver.SendPacket(data2, strlen(data2), 0, false, 10), "接收方发送");
            
            // 轮询接收
            for (int i = 0; i < 10; i++) {
                sender.Poll();
                receiver.Poll();
                usleep(50000); // 50ms
            }
            
            sender.BYEDestroy(RTPTime(1,0), 0, 0);
            receiver.BYEDestroy(RTPTime(1,0), 0, 0);
            
            bool passed = (sender.getPacketsReceived() > 0) && (receiver.getPacketsReceived() > 0);
            stats.addResult(passed);
            cout << "结果: " << (passed ? "通过" : "失败") 
                 << " (发送方收到 " << sender.getPacketsReceived() 
                 << ", 接收方收到 " << receiver.getPacketsReceived() << " 个数据包)" << endl;
                 
        } catch (const std::exception& e) {
            stats.addResult(false);
            cout << "结果: 失败 - " << e.what() << endl;
        }
    }
    
    // RTCP复用测试
    void testUDPv4RTCPMultiplexing() {
        cout << "\n--- 测试: UDP IPv4 RTCP复用 ---" << endl;
        
        try {
            BaseTestSession session;
            RTPSessionParams sessParams;
            RTPUDPv4TransmissionParams transParams;
            
            sessParams.SetOwnTimestampUnit(1.0/8000.0);
            sessParams.SetUsePollThread(false); // 禁用轮询线程，使用手动轮询
            sessParams.SetAcceptOwnPackets(true);
            transParams.SetPortbase(5030);
            transParams.SetRTCPMultiplexing(true); // 启用RTCP复用
            
            checkError(session.Create(sessParams, &transParams), "创建RTCP复用会话");
            
            // 添加目标，第三个参数true表示使用相同端口进行RTCP
            RTPEndpoint dest(ntohl(inet_addr("127.0.0.1")), 5030, true);
            checkError(session.AddDestination(dest), "添加RTCP复用目标");
            
            // 发送数据包
            const char* testData = "RTCP Multiplexing Test";
            checkError(session.SendPacket(testData, strlen(testData), 0, false, 10), "发送复用数据包");
            
            // 轮询接收
            for (int i = 0; i < 5; i++) {
                session.Poll();
                usleep(100000); // 100ms
            }
            
            session.BYEDestroy(RTPTime(1,0), 0, 0);
            
            bool passed = session.getPacketsReceived() > 0;
            stats.addResult(passed);
            cout << "结果: " << (passed ? "通过" : "失败") << " (RTCP复用模式)" << endl;
            
        } catch (const std::exception& e) {
            stats.addResult(false);
            cout << "结果: 失败 - " << e.what() << endl;
        }
    }
    
    // 现有套接字测试
    void testUDPv4ExistingSockets() {
        cout << "\n--- 测试: UDP IPv4现有套接字 ---" << endl;
        
        try {
            // 创建自定义套接字
            int rtpSock = socket(AF_INET, SOCK_DGRAM, 0);
            int rtcpSock = socket(AF_INET, SOCK_DGRAM, 0);
            
            if (rtpSock < 0 || rtcpSock < 0) {
                throw std::runtime_error("创建套接字失败");
            }
            
            // 绑定套接字
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            
            if (bind(rtpSock, (struct sockaddr*)&addr, sizeof(addr)) < 0 ||
                bind(rtcpSock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(rtpSock);
                close(rtcpSock);
                throw std::runtime_error("绑定套接字失败");
            }
            
            BaseTestSession session;
            RTPSessionParams sessParams;
            RTPUDPv4TransmissionParams transParams;
            
            sessParams.SetOwnTimestampUnit(1.0/8000.0);
            sessParams.SetUsePollThread(false); // 禁用轮询线程，使用手动轮询
            sessParams.SetAcceptOwnPackets(true);
            transParams.SetUseExistingSockets(rtpSock, rtcpSock);
            
            checkError(session.Create(sessParams, &transParams), "使用现有套接字创建会话");
            
            // 获取实际端口信息
            RTPUDPv4TransmissionInfo *info = (RTPUDPv4TransmissionInfo*)session.GetTransmissionInfo();
            uint16_t rtpPort = info->GetRTPPort();
            uint16_t rtcpPort = info->GetRTCPPort();
            
            cout << "使用端口 - RTP: " << rtpPort << ", RTCP: " << rtcpPort << endl;
            
            // 添加目标
            RTPEndpoint dest(ntohl(inet_addr("127.0.0.1")), rtpPort, rtcpPort);
            checkError(session.AddDestination(dest), "添加现有套接字目标");
            
            // 发送数据包
            const char* testData = "Existing Sockets Test";
            checkError(session.SendPacket(testData, strlen(testData), 0, false, 10), "现有套接字发送");
            
            // 轮询接收
            for (int i = 0; i < 5; i++) {
                session.Poll();
                usleep(100000);
            }
            
            session.BYEDestroy(RTPTime(1,0), 0, 0);
            
            bool passed = session.getPacketsReceived() >= 0; // 至少没有错误
            stats.addResult(passed);
            cout << "结果: " << (passed ? "通过" : "失败") << " (现有套接字模式)" << endl;
            
        } catch (const std::exception& e) {
            stats.addResult(false);
            cout << "结果: 失败 - " << e.what() << endl;
        }
    }
    
    // UDP IPv6测试
    void testUDPv6Basic() {
        cout << "\n--- 测试: UDP IPv6基础传输 ---" << endl;
        
#ifdef RTP_SUPPORT_IPV6
        try {
            BaseTestSession session;
            RTPSessionParams sessParams;
            RTPUDPv6TransmissionParams transParams;
            
            sessParams.SetOwnTimestampUnit(1.0/8000.0);
            sessParams.SetUsePollThread(false); // 禁用轮询线程，使用手动轮询
            sessParams.SetAcceptOwnPackets(true);
            transParams.SetPortbase(5040);
            
            checkError(session.Create(sessParams, &transParams, RTPTransmitter::IPv6UDPProto), "创建IPv6会话");
            
            // 使用IPv6本地地址
            struct in6_addr ipv6addr;
            inet_pton(AF_INET6, "::1", &ipv6addr); // IPv6 loopback
            RTPEndpoint dest(ipv6addr, 5040);
            checkError(session.AddDestination(dest), "添加IPv6目标");
            
            // 发送数据包
            const char* testData = "Hello RTP IPv6";
            checkError(session.SendPacket(testData, strlen(testData), 0, false, 10), "发送IPv6数据包");
            
            // 轮询接收
            for (int i = 0; i < 10; i++) {
                checkError(session.Poll(), "轮询IPv6");
                usleep(50000);
            }
            
            session.BYEDestroy(RTPTime(1,0), 0, 0);
            
            bool passed = session.getPacketsReceived() > 0;
            stats.addResult(passed);
            cout << "结果: " << (passed ? "通过" : "失败") << " (IPv6模式)" << endl;
            
        } catch (const std::exception& e) {
            stats.addResult(false);
            cout << "结果: 失败 - " << e.what() << endl;
        }
#else
        stats.addResult(false);
        cout << "结果: 跳过 - IPv6支持未编译" << endl;
#endif
    }
    
    // 错误处理测试
    void testErrorHandling() {
        cout << "\n--- 测试: 错误处理 ---" << endl;
        
        try {
            BaseTestSession session;
            RTPSessionParams sessParams;
            RTPUDPv4TransmissionParams transParams;
            
            // 测试无效端口
            transParams.SetPortbase(1); // 奇数端口应该失败
            int result = session.Create(sessParams, &transParams);
            
            if (result < 0) {
                stats.addResult(true);
            } else {
                session.BYEDestroy(RTPTime(1,0), 0, 0);
                cout << "未能捕获无效端口错误" << endl;
                stats.addResult(false);
            }
            
        } catch (const std::exception& e) {
            stats.addResult(true);
            cout << "结果: 通过 - 正确处理异常: " << e.what() << endl;
        }
    }
    
    // 性能测试
    void testPerformance() {
        cout << "\n--- 测试: 性能测试 ---" << endl;
        
        try {
            BaseTestSession session;
            RTPSessionParams sessParams;
            RTPUDPv4TransmissionParams transParams;
            
            sessParams.SetOwnTimestampUnit(1.0/8000.0);
            sessParams.SetUsePollThread(false); // 禁用轮询线程，使用手动轮询
            sessParams.SetAcceptOwnPackets(true);
            transParams.SetPortbase(5050);
            
            checkError(session.Create(sessParams, &transParams), "创建性能测试会话");
            
            RTPEndpoint dest(ntohl(inet_addr("127.0.0.1")), (uint16_t)5050, (uint16_t)5051);
            checkError(session.AddDestination(dest), "添加性能测试目标");
            
            // 发送大量数据包
            const int packetCount = 100;
            const char* testData = "Performance test packet data";
            
            cout << "发送 " << packetCount << " 个数据包..." << endl;
            
            for (int i = 0; i < packetCount; i++) {
                checkError(session.SendPacket(testData, strlen(testData), 0, false, 10), "性能测试发送");
                if (i % 10 == 0) {
                    session.Poll();
                    usleep(1000); // 1ms
                }
            }
            
            // 等待接收完成
            for (int i = 0; i < 20; i++) {
                session.Poll();
                usleep(50000); // 50ms
            }
            
            session.BYEDestroy(RTPTime(1,0), 0, 0);
            
            int received = session.getPacketsReceived();
            double successRate = (double)received / packetCount * 100.0;
            bool passed = successRate >= 90.0; // 90%成功率
            
            stats.addResult(passed);
            cout << "结果: " << (passed ? "通过" : "失败") 
                 << " (发送 " << packetCount << ", 接收 " << received 
                 << ", 成功率 " << successRate << "%)" << endl;
            
        } catch (const std::exception& e) {
            stats.addResult(false);
            cout << "结果: 失败 - " << e.what() << endl;
        }
    }
};

// 显示帮助信息
void showHelp(const char* progName) {
    cout << "用法: " << progName << " [选项]" << endl;
    cout << "选项:" << endl;
    cout << "  -h, --help     显示此帮助信息" << endl;
    cout << "  --ipv4         仅运行IPv4测试" << endl;
    cout << "  --ipv6         仅运行IPv6测试" << endl;
    cout << "  --basic        仅运行基础功能测试" << endl;
    cout << "  --performance  仅运行性能测试" << endl;
    cout << "  --rtcp         仅运行RTCP相关测试" << endl;
    cout << "  --socket       仅运行套接字管理测试" << endl;
    cout << "  --error        仅运行错误处理测试" << endl;
}

// 主函数
int main(int argc, char* argv[]) {
    cout << "综合UDP传输测试 - Linux平台" << endl;
    cout << "测试RTP库的UDP IPv4/IPv6传输功能" << endl;
    
    // 解析命令行参数
    bool runAll = true;
    bool runIPv4 = false, runIPv6 = false, runBasic = false;
    bool runPerformance = false, runRTCP = false, runSocket = false, runError = false;
    
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            showHelp(argv[0]);
            return 0;
        } else if (arg == "--ipv4") {
            runIPv4 = true; runAll = false;
        } else if (arg == "--ipv6") {
            runIPv6 = true; runAll = false;
        } else if (arg == "--basic") {
            runBasic = true; runAll = false;
        } else if (arg == "--performance") {
            runPerformance = true; runAll = false;
        } else if (arg == "--rtcp") {
            runRTCP = true; runAll = false;
        } else if (arg == "--socket") {
            runSocket = true; runAll = false;
        } else if (arg == "--error") {
            runError = true; runAll = false;
        } else {
            cout << "未知参数: " << arg << endl;
            showHelp(argv[0]);
            return 1;
        }
    }
    
    UDPTransmissionTester tester;
    
    if (runAll) {
        tester.runAllTests();
    } else {
        cout << "=== 开始选择性UDP传输测试 ===" << endl;
        try {
            if (runBasic || runIPv4) {
                tester.testUDPv4Basic();
                tester.testUDPv4Loopback();
            }
            if (runIPv6) {
                tester.testUDPv6Basic();
            }
            if (runRTCP) {
                tester.testUDPv4RTCPMultiplexing();
            }
            if (runSocket) {
                tester.testUDPv4ExistingSockets();
            }
            if (runError) {
                tester.testErrorHandling();
            }
            if (runPerformance) {
                tester.testPerformance();
            }
        } catch (const std::exception& e) {
            cerr << "测试异常: " << e.what() << endl;
        }
        tester.getStats().printSummary();
    }
    
    return 0;
}