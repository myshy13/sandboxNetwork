#include <cstdlib>
#include <memory>
#include <string>

#include <emscripten.h>

#include "Auth/tokenStore.hpp"

EM_JS(char*, js_token_get, (), {
  try {
    const token = localStorage.getItem("sandboxNetwork.token");
    return stringToNewUTF8(token || "");
  } catch (_) {
    return stringToNewUTF8("");
  }
});

EM_JS(int, js_token_set, (const char* token), {
  try {
    localStorage.setItem("sandboxNetwork.token", UTF8ToString(token));
    return 1;
  } catch (_) {
    return 0;
  }
});

EM_JS(void, js_token_remove, (), {
  try {
    localStorage.removeItem("sandboxNetwork.token");
  } catch (_) {
  }
});

class TokenStoreWeb final : public TokenStore {
 public:
  std::string load() override {
    char* value = js_token_get();
    if (value == nullptr) return "";
    std::string token(value);
    std::free(value);
    return isValidToken(token) ? token : "";
  }

  bool save(const std::string& token) override {
    return js_token_set(token.c_str()) != 0;
  }

  void clear() override { js_token_remove(); }
};

std::unique_ptr<TokenStore> makeTokenStore() {
  return std::make_unique<TokenStoreWeb>();
}
