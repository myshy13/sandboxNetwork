#include <cstdlib>
#include <filesystem>
#include <fstream>
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
    tokenPath = std::string(HOMEDIR) + std::string(FOLDER);
  };

  bool save(const std::string& token) override {
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
    std::filesystem::rename(tempPath, tokenPath / "token", ec);
    if (ec) {
      std::filesystem::remove(tokenPath / "token.tmp", ec);
      std::filesystem::remove(tokenPath / "token", ec);
    }
    return true;
  }
};