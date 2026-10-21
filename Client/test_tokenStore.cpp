#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "Auth/tokenStore.hpp"

namespace fs = std::filesystem;

// Writes raw bytes to <home>/.sandboxNetwork/token, as a hand edit or a corrupt disk would.
static void writeRaw(const fs::path& home, const std::string& content) {
  fs::create_directories(home / ".sandboxNetwork");
  std::ofstream(home / ".sandboxNetwork" / "token") << content;
}

int main() {
  // The store reads $HOME when constructed, so a temp folder keeps the real token safe.
  const fs::path home = fs::temp_directory_path() / "token_store_test";
  fs::remove_all(home);
  fs::create_directories(home);
  setenv("HOME", home.c_str(), 1);

  const std::string tokenA(43, 'a');
  const std::string tokenB = std::string(42, 'b') + "-";
  auto store = makeTokenStore();

  // ==== a missing file is "no token" ==== //
  assert(store->load().empty());

  // ==== save then load round-trips, and the file is owner-only ==== //
  assert(store->save(tokenA));
  assert(store->load() == tokenA);
  const auto perms = fs::status(home / ".sandboxNetwork" / "token").permissions();
  assert((perms & (fs::perms::group_all | fs::perms::others_all)) == fs::perms::none);

  // ==== a second save replaces the first and leaves no temp file ==== //
  assert(store->save(tokenB));
  assert(store->load() == tokenB);
  assert(!fs::exists(home / ".sandboxNetwork" / "token.tmp"));

  // ==== clear then load gives empty, and clearing twice is fine ==== //
  store->clear();
  assert(store->load().empty());
  store->clear();

  // ==== surrounding whitespace is trimmed ==== //
  writeRaw(home, "  " + tokenA + " \r\n");
  assert(store->load() == tokenA);

  // ==== only the first line counts ==== //
  writeRaw(home, tokenA + "\n" + tokenB + "\n");
  assert(store->load() == tokenA);

  // ==== garbage, wrong length and bad characters give empty ==== //
  writeRaw(home, "");
  assert(store->load().empty());
  writeRaw(home, "   \n");
  assert(store->load().empty());
  writeRaw(home, "not a token");
  assert(store->load().empty());
  writeRaw(home, std::string(42, 'a'));
  assert(store->load().empty());
  writeRaw(home, std::string(44, 'a'));
  assert(store->load().empty());
  writeRaw(home, std::string(42, 'a') + "+");
  assert(store->load().empty());
  writeRaw(home, std::string(42, 'a') + "=");
  assert(store->load().empty());

  // ==== a failed save leaves the old token ==== //
  assert(store->save(tokenA));
  fs::create_directory(home / ".sandboxNetwork" / "token.tmp");  // blocks the temp file
  assert(!store->save(tokenB));
  assert(store->load() == tokenA);
  fs::remove_all(home / ".sandboxNetwork" / "token.tmp");

  // ==== no home folder: save refuses, load is empty, clear does nothing ==== //
  unsetenv("HOME");
  unsetenv("USERPROFILE");
  auto homeless = makeTokenStore();
  assert(!homeless->save(tokenA));
  assert(homeless->load().empty());
  homeless->clear();

  fs::remove_all(home);
  std::printf("tokenStore tests passed\n");
  return 0;
}
