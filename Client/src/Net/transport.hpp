#pragma once

#include <memory>
#include <optional>
#include <string>

// One connection to the game server.
//
// Two implementations exist - ENet/UDP for native builds and WebSocket for
// Emscripten - and CMake compiles exactly one of them into the binary. That
// keeps the preprocessor out of the game code entirely: nothing above this
// layer knows or cares which transport it got.
class Transport {
public:
  virtual ~Transport() = default;

  // Non-blocking. Browsers can't block on a connect, so neither do we:
  // fire it off and watch isConnected().
  virtual void connect(const std::string &host, int port) = 0;
  virtual bool isConnected() const = 0;

  // `reliable` is a hint. Transports that physically cannot drop a packet
  // (WebSocket, being TCP) ignore it and deliver everything reliably.
  virtual void send(const std::string &bytes, bool reliable) = 0;

  // Pumps the transport and returns the next inbound message, if any.
  // This is what drives the connection state machine, so call it every
  // frame - in a loop, until it returns nullopt.
  virtual std::optional<std::string> receive() = 0;
};

// Defined by whichever backend CMake compiled in.
std::unique_ptr<Transport> makeTransport();
