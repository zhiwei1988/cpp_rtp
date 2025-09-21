#include <gtest/gtest.h>

#include "packets/media_rtp_packet_factory.h"
#include "utils/media_rtp_structs.h"
#include "utils/media_rtp_errors.h"
#include "test_utils.h"

TEST(RTPPacketTest, ParseValidWithExtensionAndCSRC) {
  auto buf = BuildRTPRaw(/*marker*/true, /*pt*/96, /*seq*/0x1234, /*ts*/0x01020304,
                         /*ssrc*/0xAABBCCDD,
                         /*csrcs*/{0x11111111, 0x22222222},
                         /*hasExt*/true, /*extId*/0xBEEF,
                         /*extData*/std::vector<uint8_t>({0xDE,0xAD,0xBE,0xEF}),
                         /*payload*/std::vector<uint8_t>({1,2,3,4,5}));

  RTPTime t(0, 0);
  uint8_t *copy = new uint8_t[buf.size()];
  std::memcpy(copy, buf.data(), buf.size());
  RTPRawPacket raw(copy, buf.size(), nullptr, t, true);
  RTPPacket p(raw);
  ASSERT_EQ(p.GetCreationError(), 0);
  EXPECT_TRUE(p.HasMarker());
  EXPECT_TRUE(p.HasExtension());
  EXPECT_EQ(p.GetPayloadType(), 96);
  EXPECT_EQ(p.GetSequenceNumber(), 0x1234);
  EXPECT_EQ(p.GetTimestamp(), 0x01020304u);
  EXPECT_EQ(p.GetSSRC(), 0xAABBCCDDu);
  EXPECT_EQ(p.GetCSRCCount(), 2);
  EXPECT_EQ(p.GetCSRC(0), 0x11111111u);
  EXPECT_EQ(p.GetCSRC(1), 0x22222222u);
  EXPECT_EQ(p.GetExtensionID(), 0xBEEFu);
  EXPECT_EQ(p.GetExtensionLength(), 4u);
  EXPECT_EQ(p.GetPayloadLength(), 5u);
}

TEST(RTPPacketTest, InvalidVersionAndSRRRHeuristic) {
  // version 错误
  auto bad = BuildRTPRaw(false, 96, 1, 2, 3);
  bad[0] &= 0x3F; // 版本清零
  RTPTime t(0, 0);
  uint8_t *bad_copy = new uint8_t[bad.size()];
  std::memcpy(bad_copy, bad.data(), bad.size());
  RTPRawPacket raw1(bad_copy, bad.size(), nullptr, t, true);
  RTPPacket p1(raw1);
  EXPECT_EQ(p1.GetCreationError(), MEDIA_RTP_ERR_PROTOCOL_ERROR);

  // marker=1 且 pt=72/73（200/201 & 127） => 误判为 RTCP，解析报错
  auto bad2 = BuildRTPRaw(true, (uint8_t)(200 & 127), 10, 20, 30);
  uint8_t *bad2_copy = new uint8_t[bad2.size()];
  std::memcpy(bad2_copy, bad2.data(), bad2.size());
  RTPRawPacket raw2(bad2_copy, bad2.size(), nullptr, t, true);
  RTPPacket p2(raw2);
  EXPECT_EQ(p2.GetCreationError(), MEDIA_RTP_ERR_PROTOCOL_ERROR);

  auto bad3 = BuildRTPRaw(true, (uint8_t)(201 & 127), 10, 20, 30);
  uint8_t *bad3_copy = new uint8_t[bad3.size()];
  std::memcpy(bad3_copy, bad3.data(), bad3.size());
  RTPRawPacket raw3(bad3_copy, bad3.size(), nullptr, t, true);
  RTPPacket p3(raw3);
  EXPECT_EQ(p3.GetCreationError(), MEDIA_RTP_ERR_PROTOCOL_ERROR);
}

TEST(RTPPacketTest, ExternalBufferConstructor) {
  // 使用外部缓冲区构造
  uint8_t buffer[1500] = {0};
  RTPPacket p(/*pt*/97, /*payload*/"data", /*len*/4, /*seq*/0xF00D,
              /*ts*/0xABCDEF12, /*ssrc*/0xCAFEBABE, /*mark*/false,
              /*numcsrcs*/0, /*csrcs*/nullptr,
              /*gotext*/false, /*extid*/0, /*extlen*/0,
              /*extdata*/nullptr,
              /*buffer*/buffer, /*buffersize*/sizeof(buffer));
  EXPECT_EQ(p.GetCreationError(), 0);
  EXPECT_EQ(p.GetPayloadType(), 97);
  EXPECT_EQ(p.HasExtension(), false);
  EXPECT_EQ(p.GetPayloadLength(), 4u);
}

TEST(RTPPacketTest, TruncatedExtensionCausesError) {
  // 构造一个扩展长度声明过大但数据不足的 RTP
  // 手动写二进制：v=2, X=1; CSRC=0
  std::vector<uint8_t> buf;
  buf.reserve(12 + 4 + 4);
  buf.push_back((2u<<6) | (1u<<4)); // V=2, X=1
  buf.push_back(96);
  buf.push_back(0); buf.push_back(1); // seq=1
  buf.push_back(0); buf.push_back(0); buf.push_back(0); buf.push_back(1); // ts
  buf.push_back(0); buf.push_back(0); buf.push_back(0); buf.push_back(1); // ssrc
  // ext header: id=0x1111, length(words)=2，但只提供 4 字节数据 => 不足
  buf.push_back(0x11); buf.push_back(0x11);
  buf.push_back(0x00); buf.push_back(0x02);
  // 仅1个word数据
  buf.push_back(0xDE); buf.push_back(0xAD); buf.push_back(0xBE); buf.push_back(0xEF);

  RTPTime t(0, 0);
  uint8_t *copy2 = new uint8_t[buf.size()];
  std::memcpy(copy2, buf.data(), buf.size());
  RTPRawPacket raw(copy2, buf.size(), nullptr, t, true);
  RTPPacket p(raw);
  EXPECT_EQ(p.GetCreationError(), MEDIA_RTP_ERR_PROTOCOL_ERROR);
}
