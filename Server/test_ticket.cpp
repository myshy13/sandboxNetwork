#include "Auth/base64.hpp"
#include "Auth/tickets.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <string>

// ==== vectors made by Auth/Tickets/ticketSigner.ts with a throwaway key; signed at now = 1000000 ==== //
const std::array<uint8_t, 32> GOOD_KEY = {0xf8, 0x9a, 0xa0, 0x2a, 0xca, 0x4e, 0xcb, 0x23, 0x34, 0x15, 0xfe, 0x89, 0x83, 0x5d, 0x6e, 0xd1, 0x01, 0x9c, 0xfa, 0x28, 0x66, 0x78, 0xa8, 0xd1, 0xe9, 0x75, 0x68, 0x87, 0x1c, 0x41, 0x32, 0xad};
const std::string ADMIN_TICKET = "AAAABwAPUFABBmhhbWlzaKbdHxVbYcqRXy0PVoe6R3JQF5PLfGbtSrGUz8RvxCfsMOWdTNY2FeYBjTwX3OJzT1NZXoXwOHk1SQGqrucP2QQ=";
const std::string USER_TICKET = "EjRWeAAPUFAABGFfQjlxcSu7Xe3ZUV4a8tNkDgpW+4XQU92jnnC4XKV9gd+/d95/JccEdVQVRmvJFTm0aEd6NltCqyXFU87JCy7MAFIG";
const std::string WRONG_KEY_TICKET = "AAAABwAPUFABBmhhbWlzaB0G4oTWa9slROKUColG1iPveJgOmNcR1ySrq/yocpykz2mbAK0yYzFVJtIyIV/KwmQDknlPe6lKFvg/RtbRJgk=";
const std::string BAD_NAME_TICKET = "AAAABwAPUFAACGJhZCBuYW1lpwt8Zl/12kOm6Ch9Wuulm7PqO4ZCntTDPkUytry6E68ElXxvnXv/1AHOP3bep7lOvAn1kbn4/UO+yFN+hDCMAQ==";
const std::string FLIPPED_ID = "AAAABgAPUFABBmhhbWlzaKbdHxVbYcqRXy0PVoe6R3JQF5PLfGbtSrGUz8RvxCfsMOWdTNY2FeYBjTwX3OJzT1NZXoXwOHk1SQGqrucP2QQ=";
const std::string FLIPPED_ADMIN = "EjRWeAAPUFABBGFfQjlxcSu7Xe3ZUV4a8tNkDgpW+4XQU92jnnC4XKV9gd+/d95/JccEdVQVRmvJFTm0aEd6NltCqyXFU87JCy7MAFIG";
const std::string FLIPPED_NAME = "AAAABwAPUFABBmlhbWlzaKbdHxVbYcqRXy0PVoe6R3JQF5PLfGbtSrGUz8RvxCfsMOWdTNY2FeYBjTwX3OJzT1NZXoXwOHk1SQGqrucP2QQ=";
const std::string FLIPPED_SIG = "AAAABwAPUFABBmhhbWlzaKbdHxVbYcqRXy0PVoe6R3JQF5PLfGbtSrGUz8RvxCfsMOWdTNY2FeYBjTwX3OJzT1NZXoXwOHk1SQGqrucP2QU=";
const std::string FLIPPED_EXP = "AAAABwAPUFEBBmhhbWlzaKbdHxVbYcqRXy0PVoe6R3JQF5PLfGbtSrGUz8RvxCfsMOWdTNY2FeYBjTwX3OJzT1NZXoXwOHk1SQGqrucP2QQ=";
const std::string TRUNCATED = "AAAABwAPUFABBmhhbWlzaKbdHxVbYcqRXy0PVoe6R3JQF5PLfGbtSrGUz8RvxCfsMOWdTNY2FeYBjTwX3OJzT1NZXoXwOHk1SQGqrucP2Q==";

constexpr int64_t SIGNED_AT = 1'000'000;
constexpr int64_t EXPIRES   = SIGNED_AT + 3600;

static std::string text(const std::vector<uint8_t> &v) { return std::string(v.begin(), v.end()); }

int main() {
  // ==== base64: the RFC 4648 vectors ==== //
  assert(text(*auth::base64Decode("")) == "");
  assert(text(*auth::base64Decode("Zg==")) == "f");
  assert(text(*auth::base64Decode("Zm8=")) == "fo");
  assert(text(*auth::base64Decode("Zm9v")) == "foo");
  assert(text(*auth::base64Decode("Zm9vYg==")) == "foob");
  assert(text(*auth::base64Decode("Zm9vYmE=")) == "fooba");
  assert(text(*auth::base64Decode("Zm9vYmFy")) == "foobar");

  // ==== base64: bad length, bad character, padding in the wrong place ==== //
  for (const char *bad : {"Zg=", "Zg", "Zm9vY", "Z===", "Zm9v====", "Zm=9", "Zm9v!A==", "Zg==Zg==", "Zm9v Zm9v", "Zm9v-_=="})
    assert(!auth::base64Decode(bad).has_value());

  // ==== a good ticket gives back exactly what auth signed ==== //
  const Tickets tickets(GOOD_KEY);
  auto admin = tickets.verify(ADMIN_TICKET, SIGNED_AT);
  assert(admin.has_value());
  assert(admin->id == 7 && admin->name == "hamish" && admin->isAdmin && admin->exp == EXPIRES);

  // ==== big-endian id, isAdmin false, the whole name alphabet ==== //
  auto user = tickets.verify(USER_TICKET, SIGNED_AT);
  assert(user.has_value());
  assert(user->id == 0x12345678u && user->name == "a_B9" && !user->isAdmin);

  // ==== expires at exp, like the Node verifier ==== //
  assert(tickets.verify(ADMIN_TICKET, EXPIRES - 1).has_value());
  assert(!tickets.verify(ADMIN_TICKET, EXPIRES).has_value());
  assert(!tickets.verify(ADMIN_TICKET, EXPIRES + 100000).has_value());

  // ==== any changed signed byte, or a different key, fails the signature ==== //
  assert(!tickets.verify(FLIPPED_ID, SIGNED_AT).has_value());
  assert(!tickets.verify(FLIPPED_EXP, SIGNED_AT).has_value());
  assert(!tickets.verify(FLIPPED_NAME, SIGNED_AT).has_value());
  assert(!tickets.verify(FLIPPED_ADMIN, SIGNED_AT).has_value());
  assert(!tickets.verify(FLIPPED_SIG, SIGNED_AT).has_value());
  assert(!tickets.verify(WRONG_KEY_TICKET, SIGNED_AT).has_value());

  std::array<uint8_t, 32> otherKey = GOOD_KEY;
  otherKey[31] ^= 1;
  assert(!Tickets(otherKey).verify(ADMIN_TICKET, SIGNED_AT).has_value());

  // ==== a validly signed name outside [A-Za-z0-9_] is still refused ==== //
  assert(!tickets.verify(BAD_NAME_TICKET, SIGNED_AT).has_value());

  // ==== malformed input never crashes and is refused ==== //
  assert(!tickets.verify("", SIGNED_AT).has_value());
  assert(!tickets.verify(TRUNCATED, SIGNED_AT).has_value());
  assert(!tickets.verify(ADMIN_TICKET + "AAAA", SIGNED_AT).has_value());
  assert(!tickets.verify("not base64!", SIGNED_AT).has_value());
  assert(!tickets.verify(std::string(100, 'A'), SIGNED_AT).has_value());
  assert(!tickets.verify(std::string(4000, 'A'), SIGNED_AT).has_value());

  std::printf("ticket tests passed\n");
  return 0;
}
