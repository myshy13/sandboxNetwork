#include <raylib.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#include "tokenStore.hpp"

class TokenStoreFile : public TokenStore {
 private:
  std::filesystem::path tokenPath;

 public:
  TokenStoreFile() {
    const char* HOMEDIR = getenv("HOME");
    const char* FOLDER = "/.sandboxNetwork/";
    if (HOMEDIR == nullptr) {
      HOMEDIR = getenv("USERPROFILE");
    }
    // No home directory: leave tokenPath empty so save() refuses.
    if (HOMEDIR == nullptr) return;
    tokenPath = std::string(HOMEDIR) + std::string(FOLDER);
  };

  bool save(const std::string& token) override {
    if (tokenPath.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(tokenPath, ec);

    if (ec) return false;

    std::filesystem::path tempPath = tokenPath / "token.tmp";
    std::ofstream tempFile(tempPath);
    // Owner-only before the token is written, so it is never world-readable.
    std::filesystem::permissions(tokenPath / "token.tmp",
                                 std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, ec);
    if (ec) {
      std::filesystem::remove(tokenPath / "token.tmp", ec);
      return false;
    }
    tempFile << token;
    // Close flushes the data; check it landed before the rename publishes it.
    tempFile.close();
    if (tempFile.fail()) {
      std::filesystem::remove(tempPath, ec);
      return false;
    }
    std::filesystem::rename(tempPath, tokenPath / "token", ec);
    if (ec) {
      std::filesystem::remove(tokenPath / "token.tmp", ec);
      return false;
    }
    return true;
  }

  void clear() override {
    if (tokenPath.empty()) return;
    // A missing file is already cleared, so errors are ignored.
    std::error_code ec;
    std::filesystem::remove(tokenPath / "token.tmp", ec);
    std::filesystem::remove(tokenPath / "token", ec);
  };

  std::string load() override {
    if (tokenPath.empty()) return "";
    std::ifstream file(tokenPath / "token");
    if (!file.is_open()) return "";

    std::string token;
    std::getline(file, token);

    // Trim surrounding whitespace (a hand-edited file may have some).
    const size_t first = token.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    token = token.substr(first, token.find_last_not_of(" \t\r\n") - first + 1);

    if (!isValidToken(token)) return "";
    return token;
  };
};

std::unique_ptr<TokenStore> makeTokenStore() {
  return std::make_unique<TokenStoreFile>();
}