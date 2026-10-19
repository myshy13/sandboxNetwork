#pragma once

#include <memory>
#include <string>

class TokenStore {
 public:
  virtual ~TokenStore() = default;
  virtual std::string load() = 0;
  virtual bool save(const std::string& token) = 0;
  virtual void clear() = 0;
};

std::unique_ptr<TokenStore> makeTokenStore();