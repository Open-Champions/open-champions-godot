#pragma once
#include <cstdint>
#include <vector>
namespace dispatch_fixture {
inline std::vector<std::uint8_t> clip_bytes(unsigned length) {
  std::vector<std::uint8_t> out(0x80, 0);
  auto word = [&](unsigned at, unsigned v) {
    for (unsigned i = 0; i < 4; ++i)
      out[at + i] = std::uint8_t(v >> (8 * i));
  };
  word(0, 1);
  word(4, length);
  word(8, 0x80);
  word(12, 0x78);
  word(16, 0x70);
  std::vector<unsigned> bits;
  auto put = [&](unsigned v, unsigned n) {
    for (unsigned i = n; i > 0; --i)
      bits.push_back((v >> (i - 1)) & 1);
  };
  put(0, 4);
  put(0, 1);
  put(0, 1);
  put(0, 1);
  put(13, 4);
  put(4096, 14);
  put(0, 14);
  put(0, 14);
  put(0, 14);
  put(255, 8);
  while (bits.size() % 32)
    bits.push_back(0);
  for (unsigned i = 0; i < bits.size(); i += 16) {
    unsigned v = 0;
    for (unsigned j = 0; j < 16; ++j)
      v = (v << 1) | bits[i + j];
    out.push_back(v & 255);
    out.push_back(v >> 8);
  }
  return out;
}
} // namespace dispatch_fixture
