// peekType() below doesn't touch Vector3, but protocol.hpp needs the
// name defined to parse (Client provides raylib's, Server provides
// models.hpp's) - this TU is compiled standalone, so give it a local one.
struct Vector3 { float x, y, z; };
#include "Protocol/protocol.hpp"
#include <string>

proto::Type proto::peekType(const std::string &data) {
  return static_cast<Type>(static_cast<uint8_t>(data[0]));
}
