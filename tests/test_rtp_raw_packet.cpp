#include <gtest/gtest.h>

#include "packets/media_rtp_packet_factory.h"
#include "utils/media_rtp_structs.h"
#include "utils/media_rtp_endpoint.h"
#include "utils/media_rtp_errors.h"

#include <vector>
#include <cstring>
#include <arpa/inet.h>

TEST(RTPRawPacketTest, AutoDetectRTCPAndExplicitFlag) {
  // 构造一个最小 RTCP BYE 包头（长度为一个头即可测试自动识别）
  std::vector<uint8_t> rtcp(sizeof(RTCPCommonHeader), 0);
  RTCPCommonHeader *hdr = reinterpret_cast<RTCPCommonHeader *>(rtcp.data());
  hdr->version = 2;
  hdr->padding = 0;
  hdr->count = 0;
  hdr->packettype = 200; // SR（200）在 200..204 范围内 => 应识别为 RTCP
  hdr->length = htons(0);

  RTPTime t(1, 2);
  // 传入的数据指针在 RTPRawPacket 析构时会被 delete[]，因此需拷贝到堆内存
  uint8_t *rtcp_copy = new uint8_t[rtcp.size()];
  std::memcpy(rtcp_copy, rtcp.data(), rtcp.size());
  RTPRawPacket autoDetected(rtcp_copy, rtcp.size(), nullptr, t);
  EXPECT_FALSE(autoDetected.IsRTP());
  EXPECT_EQ(autoDetected.GetDataLength(), rtcp.size());
  EXPECT_EQ(autoDetected.GetSenderAddress(), nullptr);

  // 不足 RTCP 头部长度 => 应保持 isrtp=true
  std::vector<uint8_t> tiny(2, 0);
  uint8_t *tiny_copy = new uint8_t[tiny.size()];
  std::memcpy(tiny_copy, tiny.data(), tiny.size());
  RTPRawPacket tinyAuto(tiny_copy, tiny.size(), nullptr, t);
  EXPECT_TRUE(tinyAuto.IsRTP());

  // 显式构造 rtp=true 与 rtp=false
  auto *payload = new uint8_t[8]();
  RTPRawPacket explicitRTP(payload, 8, nullptr, t, true);
  EXPECT_TRUE(explicitRTP.IsRTP());
  auto *payload2 = new uint8_t[8]();
  RTPRawPacket explicitRTCP(payload2, 8, nullptr, t, false);
  EXPECT_FALSE(explicitRTCP.IsRTP());
}

TEST(RTPRawPacketTest, ZeroDataSetDataAndSenderAndAllocate) {
  RTPTime t(3, 4);
  // 初始数据和地址
  auto *data1 = new uint8_t[8]{1,2,3,4,5,6,7,8};
  auto *addr1 = new RTPEndpoint(0x7F000001u /*127.0.0.1*/, 5004, 5005);
  RTPRawPacket p(data1, 8, addr1, t, true);

  // ZeroData 之后数据指针与长度应为 0
  p.ZeroData();
  EXPECT_EQ(p.GetData(), nullptr);
  EXPECT_EQ(p.GetDataLength(), 0u);

  // SetData 替换数据
  auto *data2 = new uint8_t[4]{9,9,9,9};
  p.SetData(data2, 4);
  EXPECT_EQ(p.GetData(), data2);
  EXPECT_EQ(p.GetDataLength(), 4u);

  // 替换发送者地址
  auto *addr2 = new RTPEndpoint(0x7F000001u, 6004, 6005);
  p.SetSenderAddress(addr2);
  ASSERT_NE(p.GetSenderAddress(), nullptr);
  EXPECT_EQ(p.GetSenderAddress()->GetRtpPort(), 6004);

  // AllocateBytes 返回可用内存
  auto *tmp = p.AllocateBytes(true, 16);
  ASSERT_NE(tmp, nullptr);
  tmp[0] = 0xAA; // 简单写入验证
  delete [] tmp;
}
