#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace auth {

// Value of one standard-alphabet base64 character, or -1 if it isn't one.
constexpr int base64Index(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

// Strict decode of padded base64 (what Node's toString("base64") makes); nullopt on any bad character, length or padding.
inline std::optional<std::vector<uint8_t>> base64Decode(std::string_view in) {
  if (in.size() % 4 != 0) return std::nullopt;

  std::vector<uint8_t> out;
  out.reserve(in.size() / 4 * 3);

  for (size_t i = 0; i < in.size(); i += 4) {
    // ==== padding: only the last group, and '=' only at the end of it ==== //
    int pad = 0;
    if (i + 4 == in.size() && in[i + 3] == '=') pad = (in[i + 2] == '=') ? 2 : 1;

    uint32_t n = 0;
    for (int j = 0; j < 4; j++) {
      int v = (j < 4 - pad) ? base64Index(in[i + j]) : 0;
      if (v < 0) return std::nullopt;
      n = (n << 6) | static_cast<uint32_t>(v);
    }

    out.push_back(static_cast<uint8_t>(n >> 16));
    if (pad < 2) out.push_back(static_cast<uint8_t>(n >> 8));
    if (pad < 1) out.push_back(static_cast<uint8_t>(n));
  }
  return out;
}

} // namespace auth
