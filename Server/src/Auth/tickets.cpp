#include "tickets.hpp"
#include "Auth/base64.hpp"
#include <cstddef>
#include <cstdint>
#include <monocypher-ed25519.h>
#include <optional>
#include <string>
#include <vector>

// ==== ticket layout (see Auth/Tickets/ticketSigner.ts) ==== //
constexpr size_t SIG_LEN = 64;
constexpr size_t HEADER_LEN = 10; // id(4) exp(4) isAdmin(1) nameLen(1)
constexpr size_t MAX_NAME_LEN = 16;

namespace {

uint32_t readU32BE(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
         (uint32_t)p[3];
}

bool validNameChar(uint8_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '_';
}

} // namespace

Tickets::Tickets(const std::array<uint8_t, 32> &pubKey) : publicKey(pubKey) {}

std::optional<Claim> Tickets::verify(const std::string &ticketBase64,
                                     int64_t nowSeconds) const {
  const std::optional<std::vector<uint8_t>> decoded =
      auth::base64Decode(ticketBase64);
  if (!decoded.has_value())
    return std::nullopt;
  const std::vector<uint8_t> &bytes = decoded.value();

  // ==== lengths first: nothing else is read until the size is exact ==== //
  if (bytes.size() < HEADER_LEN + SIG_LEN)
    return std::nullopt;
  const size_t nameLen = bytes[9];
  if (nameLen > MAX_NAME_LEN || bytes.size() != HEADER_LEN + nameLen + SIG_LEN)
    return std::nullopt;

  // ==== signature over everything before it; 0 means valid ==== //
  const size_t signedLen = bytes.size() - SIG_LEN;
  if (crypto_ed25519_check(bytes.data() + signedLen, publicKey.data(),
                           bytes.data(), signedLen) != 0)
    return std::nullopt;

  // ==== only now are the fields trusted ==== //
  const uint32_t exp = readU32BE(&bytes[4]);
  if ((int64_t)exp <= nowSeconds)
    return std::nullopt;

  for (size_t i = 0; i < nameLen; i++)
    if (!validNameChar(bytes[HEADER_LEN + i]))
      return std::nullopt;

  return Claim{
      .id = readU32BE(&bytes[0]),
      .name = std::string(bytes.begin() + HEADER_LEN,
                          bytes.begin() + HEADER_LEN + nameLen),
      .exp = exp,
      .isAdmin = bytes[8] == 1,
  };
}
