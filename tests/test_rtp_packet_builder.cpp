#include <gtest/gtest.h>

#include "packets/media_rtp_packet_factory.h"
#include "core/media_rtp_sources.h"
#include "utils/media_rtp_errors.h"
#include "utils/media_rtp_structs.h"

#include <vector>
#include <cstring>

TEST(RTPPacketBuilderTest, ApiErrorsBeforeInit) {
  RTPPacketBuilder b;
  // 未 Init 调用应报错
  EXPECT_EQ(b.SetDefaultPayloadType(96), MEDIA_RTP_ERR_INVALID_STATE);
  EXPECT_EQ(b.SetDefaultMark(false), MEDIA_RTP_ERR_INVALID_STATE);
  EXPECT_EQ(b.SetDefaultTimestampIncrement(160), MEDIA_RTP_ERR_INVALID_STATE);
  EXPECT_EQ(b.BuildPacket("x", 1), MEDIA_RTP_ERR_INVALID_STATE);
  EXPECT_EQ(b.BuildPacketEx("x", 1, 1, "y", 1), MEDIA_RTP_ERR_INVALID_STATE);
}

TEST(RTPPacketBuilderTest, DefaultsAndTimestampIncrement) {
  RTPPacketBuilder b;
  ASSERT_EQ(b.Init(512), 0);

  // 未设置默认时间戳增量时 IncrementTimestampDefault 报协议错误
  EXPECT_EQ(b.IncrementTimestampDefault(), MEDIA_RTP_ERR_PROTOCOL_ERROR);

  ASSERT_EQ(b.SetDefaultPayloadType(96), 0);
  ASSERT_EQ(b.SetDefaultMark(true), 0);
  ASSERT_EQ(b.SetDefaultTimestampIncrement(160), 0);

  // 记录初始 seq/timestamp
  uint16_t seq0 = b.GetSequenceNumber();
  uint32_t ts0 = b.GetTimestamp();

  // 使用默认参数构建
  std::vector<uint8_t> pl1(20, 0x11);
  ASSERT_EQ(b.BuildPacket(pl1.data(), pl1.size()), 0);
  EXPECT_EQ(b.GetSequenceNumber(), (uint16_t)(seq0 + 1));
  EXPECT_EQ(b.GetTimestamp(), ts0 + 160u);

  // 再构建一次（默认）
  ASSERT_EQ(b.BuildPacket(pl1.data(), pl1.size()), 0);
  EXPECT_EQ(b.GetSequenceNumber(), (uint16_t)(seq0 + 2));
  EXPECT_EQ(b.GetTimestamp(), ts0 + 320u);

  b.Destroy();
}

TEST(RTPPacketBuilderTest, ExplicitAndExtensionAndRoundTrip) {
  RTPPacketBuilder b;
  ASSERT_EQ(b.Init(1500), 0);

  // 添加 CSRC 列表，去重检验
  ASSERT_EQ(b.AddCSRC(0xABCDEF01), 0);
  ASSERT_EQ(b.AddCSRC(0xABCDEF02), 0);
  EXPECT_EQ(b.AddCSRC(0xABCDEF01), MEDIA_RTP_ERR_INVALID_STATE); // duplicate

  // 显式参数构建 + 扩展头
  std::vector<uint8_t> payload(10, 0x42);
  std::vector<uint32_t> csrcs = {0xABCDEF01, 0xABCDEF02};
  uint16_t extId = 0x1001;
  uint32_t inc = 90;

  ASSERT_EQ(b.BuildPacketEx(payload.data(), payload.size(), 97, true, inc, extId,
                             "EXTD", 1 /*one 32-bit word*/), 0);

  // 使用 RTPPacket 解析并校验字段
  RTPTime now(0, 0);
  size_t L = b.GetPacketLength();
  uint8_t *copy = new uint8_t[L];
  std::memcpy(copy, b.GetPacket(), L);
  RTPRawPacket raw(copy, L, nullptr, now, true);
  RTPPacket p(raw);
  ASSERT_EQ(p.GetCreationError(), 0);
  EXPECT_TRUE(p.HasMarker());
  EXPECT_TRUE(p.HasExtension());
  EXPECT_EQ(p.GetPayloadType(), 97);
  EXPECT_EQ(p.GetCSRCCount(), 2);
  EXPECT_EQ(p.GetCSRC(0), 0xABCDEF01u);
  EXPECT_EQ(p.GetCSRC(1), 0xABCDEF02u);
  EXPECT_EQ(p.GetExtensionID(), extId);
  EXPECT_EQ(p.GetExtensionLength(), 4u);
}

TEST(RTPPacketBuilderTest, MaxPacketSizeExceeded) {
  RTPPacketBuilder b;
  ASSERT_EQ(b.Init(64), 0);
  ASSERT_EQ(b.SetDefaultPayloadType(96), 0);
  ASSERT_EQ(b.SetDefaultMark(false), 0);
  ASSERT_EQ(b.SetDefaultTimestampIncrement(1), 0);

  std::vector<uint8_t> big(256, 0xEE);
  // 长度超过 maxpacksize，应返回参数错误
  EXPECT_EQ(b.BuildPacket(big.data(), big.size()), MEDIA_RTP_ERR_INVALID_PARAMETER);
}

TEST(RTPPacketBuilderTest, CreateNewSSRCWithAndWithoutSources) {
  RTPPacketBuilder b;
  ASSERT_EQ(b.Init(256), 0);

  // 无 sources
  uint32_t ssrc1 = b.CreateNewSSRC();
  EXPECT_NE(ssrc1, 0u);

  // 有 sources（包含一个已有条目），期望结果不与该条目相等
  RTPSources sources;
  ASSERT_EQ(sources.CreateOwnSSRC(0x11223344), 0);
  uint32_t ssrc2 = b.CreateNewSSRC(sources);
  EXPECT_NE(ssrc2, 0x11223344u);
  EXPECT_FALSE(sources.GotEntry(ssrc2));
}

TEST(RTPPacketBuilderTest, CSRCListBoundsAndClear) {
  RTPPacketBuilder b;
  ASSERT_EQ(b.Init(1024), 0);

  // 填满到 RTP_MAXCSRCS
  for (int i = 0; i < RTP_MAXCSRCS; ++i) {
    ASSERT_EQ(b.AddCSRC(0x1000 + (uint32_t)i), 0);
  }
  // 超出应报资源错误
  EXPECT_EQ(b.AddCSRC(0xDEADBEEF), MEDIA_RTP_ERR_RESOURCE_ERROR);

  // 构包并验证 CSRC 数量
  ASSERT_EQ(b.SetDefaultPayloadType(98), 0);
  ASSERT_EQ(b.SetDefaultMark(false), 0);
  ASSERT_EQ(b.SetDefaultTimestampIncrement(10), 0);
  uint8_t d[4] = {1,2,3,4};
  ASSERT_EQ(b.BuildPacket(d, sizeof(d)), 0);

  RTPTime now(0, 0);
  size_t L2 = b.GetPacketLength();
  uint8_t *copy2 = new uint8_t[L2];
  std::memcpy(copy2, b.GetPacket(), L2);
  RTPRawPacket raw(copy2, L2, nullptr, now, true);
  RTPPacket p(raw);
  ASSERT_EQ(p.GetCreationError(), 0);
  EXPECT_EQ(p.GetCSRCCount(), RTP_MAXCSRCS);

  // 删除一个 CSRC 并清空列表不崩溃
  EXPECT_EQ(b.DeleteCSRC(0x1000), 0);
  b.ClearCSRCList();
}
