// 测试辅助函数：原始字节构造和断言工具
#pragma once

#include <cstdint>
#include <vector>
#include <cstring>

// 构造一个最小的 RTP 原始包字节序列
// 支持：marker、payload type、seq、timestamp、ssrc、CSRC 列表、扩展头
inline std::vector<uint8_t> BuildRTPRaw(
    bool marker,
    uint8_t payloadType7,
    uint16_t seq,
    uint32_t ts,
    uint32_t ssrc,
    const std::vector<uint32_t>& csrcs = {},
    bool hasExt = false,
    uint16_t extId = 0,
    const std::vector<uint8_t>& extData = {},
    const std::vector<uint8_t>& payload = {})
{
  uint8_t vpxcc = 0;
  vpxcc |= (2u << 6); // version = 2
  if (hasExt) vpxcc |= (1u << 4);
  // padding=0
  vpxcc |= (uint8_t)(csrcs.size() & 0x0F);

  uint8_t mpt = 0;
  if (marker) mpt |= 0x80u;
  mpt |= (payloadType7 & 0x7Fu);

  std::vector<uint8_t> buf;
  buf.reserve(12 + csrcs.size()*4 + (hasExt? 4 + ((extData.size()+3)/4)*4 : 0) + payload.size());

  buf.push_back(vpxcc);
  buf.push_back(mpt);
  buf.push_back((uint8_t)((seq >> 8) & 0xFF));
  buf.push_back((uint8_t)(seq & 0xFF));
  buf.push_back((uint8_t)((ts >> 24) & 0xFF));
  buf.push_back((uint8_t)((ts >> 16) & 0xFF));
  buf.push_back((uint8_t)((ts >> 8) & 0xFF));
  buf.push_back((uint8_t)(ts & 0xFF));
  buf.push_back((uint8_t)((ssrc >> 24) & 0xFF));
  buf.push_back((uint8_t)((ssrc >> 16) & 0xFF));
  buf.push_back((uint8_t)((ssrc >> 8) & 0xFF));
  buf.push_back((uint8_t)(ssrc & 0xFF));

  for (auto csrc : csrcs) {
    buf.push_back((uint8_t)((csrc >> 24) & 0xFF));
    buf.push_back((uint8_t)((csrc >> 16) & 0xFF));
    buf.push_back((uint8_t)((csrc >> 8) & 0xFF));
    buf.push_back((uint8_t)(csrc & 0xFF));
  }

  if (hasExt) {
    // extension header: id, length (in 32-bit words)
    uint16_t words = (uint16_t)((extData.size() + 3) / 4);
    buf.push_back((uint8_t)((extId >> 8) & 0xFF));
    buf.push_back((uint8_t)(extId & 0xFF));
    buf.push_back((uint8_t)((words >> 8) & 0xFF));
    buf.push_back((uint8_t)(words & 0xFF));
    // data padded to 32-bit boundary
    size_t oldSize = buf.size();
    buf.resize(oldSize + words*4, 0);
    std::memcpy(buf.data() + oldSize, extData.data(), extData.size());
  }

  if (!payload.empty()) {
    buf.insert(buf.end(), payload.begin(), payload.end());
  }
  return buf;
}

// 构造一个最小的 RTCP SDES 包（可包含未知 item）
// 参数：ssrc, items: (id, data)
inline std::vector<uint8_t> BuildRTCPSDES(uint32_t ssrc,
                                          const std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& items)
{
  std::vector<uint8_t> body;
  // SSRC
  body.push_back((uint8_t)((ssrc >> 24) & 0xFF));
  body.push_back((uint8_t)((ssrc >> 16) & 0xFF));
  body.push_back((uint8_t)((ssrc >> 8) & 0xFF));
  body.push_back((uint8_t)(ssrc & 0xFF));
  // items
  for (auto &it : items) {
    body.push_back(it.first);
    body.push_back((uint8_t)it.second.size());
    body.insert(body.end(), it.second.begin(), it.second.end());
  }
  // end marker 0x00 and 32bit pad
  body.push_back(0);
  // pad to multiple of 4
  while (body.size() % 4 != 0) body.push_back(0);

  // RTCP header
  // v=2, p=0, count=1, PT=SDES(202), length in 32-bit words - 1
  size_t words = (sizeof(uint32_t) + body.size()) / 4; // header(4B) counted later
  std::vector<uint8_t> pkt;
  pkt.reserve(4 + body.size());
  uint8_t vpc = 0;
  vpc |= (2u << 6);
  vpc |= 1u; // count=1 chunk
  pkt.push_back(vpc);
  pkt.push_back(202); // PT=SDES
  uint16_t length = (uint16_t)(body.size()/4 + 1 /*ssrc*/);
  pkt.push_back((uint8_t)((length >> 8) & 0xFF));
  pkt.push_back((uint8_t)(length & 0xFF));
  // append body
  pkt.insert(pkt.end(), body.begin(), body.end());
  return pkt;
}

// 构造一个最小的 RTCP BYE 包，支持 reason length 0 的场景
inline std::vector<uint8_t> BuildRTCPBYE(const std::vector<uint32_t>& ssrcs,
                                         bool withReasonLenZero)
{
  std::vector<uint8_t> body;
  for (auto s : ssrcs) {
    body.push_back((uint8_t)((s >> 24) & 0xFF));
    body.push_back((uint8_t)((s >> 16) & 0xFF));
    body.push_back((uint8_t)((s >> 8) & 0xFF));
    body.push_back((uint8_t)(s & 0xFF));
  }
  if (withReasonLenZero) {
    body.push_back(0); // reason length = 0, no reason data
    // pad to multiple of 4
    while (body.size() % 4 != 0) body.push_back(0);
  }
  // header
  std::vector<uint8_t> pkt;
  uint8_t vpc = 0;
  vpc |= (2u << 6);
  vpc |= (uint8_t)(ssrcs.size() & 0x1F);
  pkt.push_back(vpc);
  pkt.push_back(203); // PT=BYE
  uint16_t length = (uint16_t)(body.size()/4 + 1); // (len+1) 32-bit words
  pkt.push_back((uint8_t)((length >> 8) & 0xFF));
  pkt.push_back((uint8_t)(length & 0xFF));
  pkt.insert(pkt.end(), body.begin(), body.end());
  return pkt;
}

