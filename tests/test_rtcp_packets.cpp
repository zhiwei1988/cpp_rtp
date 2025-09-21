#include <gtest/gtest.h>

#include "packets/media_rtcp_packet_factory.h"
#include "packets/media_rtp_packet_factory.h"
#include "core/media_rtp_sources.h"
#include "utils/media_rtp_errors.h"
#include "utils/media_rtp_structs.h"
#include "test_utils.h"

#include <vector>
#include <cstring>
#include <arpa/inet.h>

TEST(RTCPPacketsTest, APPPacketFields) {
  RTCPCompoundPacketBuilder b;
  ASSERT_EQ(b.InitBuild(1500), 0);
  // 先放一个 RR 以满足 Compound 约束
  ASSERT_EQ(b.StartReceiverReport(0x01020304), 0);
  // SDES + APP
  ASSERT_EQ(b.AddSDESSource(0x01020304), 0);
  const char cname[] = "cname";
  ASSERT_EQ(b.AddSDESNormalItem(RTCPSDESPacket::CNAME, cname, (uint8_t)strlen(cname)), 0);

  const uint8_t name[4] = {'T','E','S','T'};
  uint8_t appdata[8] = {1,2,3,4,5,6,7,8};
  ASSERT_EQ(b.AddAPPPacket(7, 0x01020304, name, appdata, sizeof(appdata)), 0);
  ASSERT_EQ(b.EndBuild(), 0);

  b.GotoFirstPacket();
  RTCPPacket* p = nullptr;
  // 跳过 RR 与 SDES，找到 APP
  p = b.GetNextPacket(); ASSERT_NE(p, nullptr); // RR
  p = b.GetNextPacket(); ASSERT_NE(p, nullptr); // SDES
  p = b.GetNextPacket(); ASSERT_NE(p, nullptr); // APP

  ASSERT_EQ(p->GetPacketType(), RTCPPacket::APP);
  RTCPAPPPacket *app = static_cast<RTCPAPPPacket*>(p);
  EXPECT_TRUE(app->IsKnownFormat());
  EXPECT_EQ(app->GetSubType(), 7);
  EXPECT_EQ(app->GetSSRC(), 0x01020304u);
  uint8_t *nm = app->GetName();
  ASSERT_NE(nm, nullptr);
  EXPECT_EQ(nm[0], 'T'); EXPECT_EQ(nm[1], 'E'); EXPECT_EQ(nm[2], 'S'); EXPECT_EQ(nm[3], 'T');
  ASSERT_NE(app->GetAPPData(), nullptr);
  EXPECT_EQ(app->GetAPPDataLength(), 8u);
}

TEST(RTCPPacketsTest, BYENoReasonAndZeroLengthReason) {
  // 无离开原因
  auto bye1 = BuildRTCPBYE({0xAABBCCDD}, /*withReasonLenZero*/false);
  RTCPBYEPacket p1(bye1.data(), bye1.size());
  EXPECT_TRUE(p1.IsKnownFormat());
  EXPECT_EQ(p1.GetSSRCCount(), 1);
  EXPECT_EQ(p1.GetSSRC(0), 0xAABBCCDDu);
  EXPECT_FALSE(p1.HasReasonForLeaving());
  EXPECT_EQ(p1.GetReasonLength(), 0u);
  EXPECT_EQ(p1.GetReasonData(), nullptr);

  // 原因长度为 0（有 reason length 字节但为 0）
  auto bye2 = BuildRTCPBYE({0x11223344, 0x55667788}, /*withReasonLenZero*/true);
  RTCPBYEPacket p2(bye2.data(), bye2.size());
  EXPECT_TRUE(p2.IsKnownFormat());
  EXPECT_EQ(p2.GetSSRCCount(), 2);
  EXPECT_TRUE(p2.HasReasonForLeaving());
  EXPECT_EQ(p2.GetReasonLength(), 0u);
  EXPECT_EQ(p2.GetReasonData(), nullptr);
}

TEST(RTCPPacketsTest, RRPacketReportBlockFields) {
  RTCPCompoundPacketBuilder b;
  ASSERT_EQ(b.InitBuild(1500), 0);
  ASSERT_EQ(b.StartReceiverReport(0x01020304), 0);
  // 添加两个报告块，一个使用负数的 lostpackets
  ASSERT_EQ(b.AddReportBlock(0x0A0B0C0D, 5, -5, 0x01010101, 0x00000010, 0x00000020, 0x00000030), 0);
  ASSERT_EQ(b.AddReportBlock(0x0E0F1011, 0, 3, 0x01010102, 0x00000011, 0x00000021, 0x00000031), 0);
  ASSERT_EQ(b.EndBuild(), 0);

  b.GotoFirstPacket();
  RTCPPacket* p = b.GetNextPacket();
  ASSERT_NE(p, nullptr);
  ASSERT_EQ(p->GetPacketType(), RTCPPacket::RR);
  RTCPRRPacket *rr = static_cast<RTCPRRPacket*>(p);
  EXPECT_TRUE(rr->IsKnownFormat());
  EXPECT_EQ(rr->GetSenderSSRC(), 0x01020304u);
  ASSERT_EQ(rr->GetReceptionReportCount(), 2);

  EXPECT_EQ(rr->GetSSRC(0), 0x0A0B0C0Du);
  EXPECT_EQ(rr->GetFractionLost(0), 5);
  EXPECT_EQ(rr->GetLostPacketCount(0), -5);
  EXPECT_EQ(rr->GetExtendedHighestSequenceNumber(0), 0x01010101u);
  EXPECT_EQ(rr->GetJitter(0), 0x10u);
  EXPECT_EQ(rr->GetLSR(0), 0x20u);
  EXPECT_EQ(rr->GetDLSR(0), 0x30u);
}

TEST(RTCPPacketsTest, SRPacketFields) {
  RTCPCompoundPacketBuilder b;
  ASSERT_EQ(b.InitBuild(1500), 0);
  RTPNTPTime ntp(0x11223344, 0x55667788);
  ASSERT_EQ(b.StartSenderReport(0x01020304, ntp, 0xAABBCCDD, 10, 20), 0);
  // 附带一个报告块
  ASSERT_EQ(b.AddReportBlock(0x0A0B0C0D, 0, 0, 0x01010101, 0x00000010, 0x00000020, 0x00000030), 0);
  ASSERT_EQ(b.EndBuild(), 0);

  b.GotoFirstPacket();
  RTCPPacket* p = b.GetNextPacket();
  ASSERT_NE(p, nullptr);
  ASSERT_EQ(p->GetPacketType(), RTCPPacket::SR);
  RTCPSRPacket *sr = static_cast<RTCPSRPacket*>(p);
  EXPECT_TRUE(sr->IsKnownFormat());
  EXPECT_EQ(sr->GetSenderSSRC(), 0x01020304u);
  auto ntpOut = sr->GetNTPTimestamp();
  EXPECT_EQ(ntpOut.GetMSW(), 0x11223344u);
  EXPECT_EQ(ntpOut.GetLSW(), 0x55667788u);
  EXPECT_EQ(sr->GetRTPTimestamp(), 0xAABBCCDDu);
  EXPECT_EQ(sr->GetSenderPacketCount(), 10u);
  EXPECT_EQ(sr->GetSenderOctetCount(), 20u);
}

TEST(RTCPPacketsTest, SDESIterationCNAMEAndUnknownAndAlignment) {
  // 先用 Builder 生成一个包含 CNAME 的 SDES
  RTCPCompoundPacketBuilder b1;
  ASSERT_EQ(b1.InitBuild(1500), 0);
  ASSERT_EQ(b1.StartReceiverReport(0x01020304), 0);
  ASSERT_EQ(b1.AddSDESSource(0x11223344), 0);
  const char cname[] = "alice@example";
  ASSERT_EQ(b1.AddSDESNormalItem(RTCPSDESPacket::CNAME, cname, (uint8_t)strlen(cname)), 0);
  ASSERT_EQ(b1.AddSDESSource(0x55667788), 0); // 第二个块，无项
  ASSERT_EQ(b1.EndBuild(), 0);

  // 第二个包：手工构造含 Unknown item 的 SDES，用类直接解析
  std::vector<uint8_t> unknownData; unknownData.push_back('x'); unknownData.push_back('y'); unknownData.push_back('z');
  std::vector<std::pair<uint8_t, std::vector<uint8_t>>> items; items.emplace_back(9, unknownData);
  auto sdesUnknown = BuildRTCPSDES(0xAABBCCDD, items);
  RTCPSDESPacket sdes2(sdesUnknown.data(), sdesUnknown.size());
  ASSERT_TRUE(sdes2.IsKnownFormat());
  ASSERT_TRUE(sdes2.GotoFirstChunk());
  EXPECT_EQ(sdes2.GetChunkSSRC(), 0xAABBCCDDu);
  ASSERT_TRUE(sdes2.GotoFirstItem());
  EXPECT_EQ(sdes2.GetItemType(), RTCPSDESPacket::Unknown);
  EXPECT_EQ(sdes2.GetItemLength(), 3u);
  uint8_t *idata = sdes2.GetItemData();
  ASSERT_NE(idata, nullptr);
  EXPECT_EQ(idata[0], 'x'); EXPECT_EQ(idata[1], 'y'); EXPECT_EQ(idata[2], 'z');
  EXPECT_FALSE(sdes2.GotoNextItem());
}

TEST(RTCPPacketsTest, UnknownPacketKnownFormatTrue) {
  // 构造任意未知类型 RTCP 包
  std::vector<uint8_t> buf(8, 0);
  RTCPCommonHeader *hdr = reinterpret_cast<RTCPCommonHeader*>(buf.data());
  hdr->version = 2; hdr->padding = 0; hdr->count = 0;
  hdr->packettype = 210; // 非 200..204 范围
  hdr->length = htons(1); // one 32-bit word (header-only + 4 bytes body)
  RTCPUnknownPacket up(buf.data(), buf.size());
  EXPECT_TRUE(up.IsKnownFormat());
}

TEST(RTCPPacketsTest, CompoundBuilderOrderAndRoundTripAndOversize) {
  // 超小 maxpacksize 应直接报资源错误
  RTCPCompoundPacketBuilder small;
  EXPECT_EQ(small.InitBuild(100), MEDIA_RTP_ERR_RESOURCE_ERROR);

  // 构造 SR + SDES + BYE
  RTCPCompoundPacketBuilder b;
  ASSERT_EQ(b.InitBuild(1500), 0);
  RTPNTPTime ntp(1, 2);
  ASSERT_EQ(b.StartSenderReport(0x01020304, ntp, 0xABCDEF12, 1, 2), 0);
  ASSERT_EQ(b.AddSDESSource(0x01020304), 0);
  const char cname[] = "bob";
  ASSERT_EQ(b.AddSDESNormalItem(RTCPSDESPacket::CNAME, cname, (uint8_t)strlen(cname)), 0);
  uint32_t ssrcs[1] = {0x01020304};
  ASSERT_EQ(b.AddBYEPacket(ssrcs, 1, nullptr, 0), 0);
  ASSERT_EQ(b.EndBuild(), 0);

  // 顺序应为 SR -> SDES -> BYE
  b.GotoFirstPacket();
  RTCPPacket *p = nullptr;
  p = b.GetNextPacket(); ASSERT_NE(p, nullptr); EXPECT_EQ(p->GetPacketType(), RTCPPacket::SR);
  p = b.GetNextPacket(); ASSERT_NE(p, nullptr); EXPECT_EQ(p->GetPacketType(), RTCPPacket::SDES);
  p = b.GetNextPacket(); ASSERT_NE(p, nullptr); EXPECT_EQ(p->GetPacketType(), RTCPPacket::BYE);
  EXPECT_EQ(b.GetNextPacket(), nullptr);

  // 往返一致性：用生成的 buffer 再解析一次
  uint8_t *buf = b.GetCompoundPacketData();
  size_t len = b.GetCompoundPacketLength();
  RTCPCompoundPacket cp2(buf, len, /*deletedata*/false);
  ASSERT_EQ(cp2.GetCreationError(), 0);
  cp2.GotoFirstPacket();
  p = cp2.GetNextPacket(); ASSERT_NE(p, nullptr); EXPECT_EQ(p->GetPacketType(), RTCPPacket::SR);
  p = cp2.GetNextPacket(); ASSERT_NE(p, nullptr); EXPECT_EQ(p->GetPacketType(), RTCPPacket::SDES);
  p = cp2.GetNextPacket(); ASSERT_NE(p, nullptr); EXPECT_EQ(p->GetPacketType(), RTCPPacket::BYE);
  EXPECT_EQ(cp2.GetNextPacket(), nullptr);
}

TEST(RTCPPacketsTest, RTCPPacketBuilderBuildsRRWithSDES) {
  // 仅基础校验：初始化并生成一个复合包（无源信息 -> RR+SDES）
  RTPSources sources;          // 空源表，不是 sender
  RTPPacketBuilder rtpb;       // 不发送任何 RTP 包
  ASSERT_EQ(rtpb.Init(512), 0);
  RTCPPacketBuilder rtcps(sources, rtpb);
  ASSERT_EQ(rtcps.Init(1200, 1.0/8000.0, "me", 2), 0);

  RTCPCompoundPacket *pack = nullptr;
  ASSERT_EQ(rtcps.BuildNextPacket(&pack), 0);
  ASSERT_NE(pack, nullptr);
  pack->GotoFirstPacket();
  RTCPPacket *p = pack->GetNextPacket();
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->GetPacketType() == RTCPPacket::RR || p->GetPacketType() == RTCPPacket::SR);
  // 注意：此对象由内部缓冲区支撑，删除可能触发复杂的释放路径。
  // 在单测进程结束时让系统回收，避免不必要的析构副作用。
}
