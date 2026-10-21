#pragma once

#include <cstddef>
#include <memory>
#include <string>

inline bool isValidToken(const std::string& token) {
  constexpr std::size_t TOKEN_LENGTH = 43;
  if (token.size() != TOKEN_LENGTH) return false;
  for (unsigned char c : token) {
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-')) {
      return false;
    }
  }
  return true;
}

class TokenStore {
 public:
  virtual ~TokenStore() = default;
  virtual std::string load() = 0;
  virtual bool save(const std::string& token) = 0;
  virtual void clear() = 0;
};

std::unique_ptr<TokenStore> makeTokenStore();