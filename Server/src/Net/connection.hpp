#pragma once

#include <string>

// One connected client, whatever transport it arrived on.
//
// The game logic only ever talks to this interface - it never touches an
// ENetPeer or a WebSocket directly, so adding a transport doesn't mean
// touching handleConnect/handleReceive/tick at all.
class Connection {
public:
  virtual ~Connection() = default;

  // `reliable` is a hint. WebSocket rides on TCP and cannot drop a message,
  // so that backend ignores the flag and delivers everything reliably.
  virtual void send(const std::string &bytes, bool reliable) = 0;
};
