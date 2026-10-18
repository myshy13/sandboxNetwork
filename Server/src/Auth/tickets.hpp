#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

struct Claim {
  uint32_t id;
  std::string name;
  uint32_t exp; // unix seconds
  bool isAdmin;
};

class Tickets {
private:
  std::array<uint8_t, 32> publicKey;

public:
  // Returns the claims only if the signature checks out and the ticket hasn't expired.
  std::optional<Claim> verify(const std::string &ticketBase64, int64_t nowSeconds) const;
  explicit Tickets(const std::array<uint8_t, 32> &publicKey);
};
