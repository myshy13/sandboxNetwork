#include "Protocol/protocol.hpp"
#include <string>

proto::Type proto::peekType(const std::string &data) {
  return static_cast<Type>(static_cast<uint8_t>(data[0]));
}
