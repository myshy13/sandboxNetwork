#pragma once

#include "Net/connection.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ix {
class WebSocket;
class WebSocketServer;
} // namespace ix

// Something that happened on a WebSocket client, handed to the game thread.
struct WsEvent {
  enum class Kind { Connect, Message, Disconnect };

  Kind kind;
  std::string socketId;
  std::string data;                       // Message only
  std::unique_ptr<Connection> connection; // Connect only
};

// Lets browsers play.
//
// Browsers cannot open raw UDP sockets, so an Emscripten client cannot speak
// ENet. This listens for WebSocket connections and republishes them as plain
// Connection/WsEvent pairs, which the server feeds through exactly the same
// handlers as its ENet clients.
//
// IXWebSocket runs its own accept/read threads, so everything crossing into
// the game thread goes through the mutex-guarded queue below.
class WsProxy {
public:
  explicit WsProxy(int port);
  ~WsProxy();

  bool isListening() const { return listening; }

  // Called from the game thread once per poll.
  std::vector<WsEvent> drain();

  // Thread-safe, and a no-op if that client has already gone away.
  void sendTo(const std::string &socketId, const std::string &bytes);

private:
  std::unique_ptr<ix::WebSocketServer> server;
  bool listening{false};

  // Guards both maps below - touched by the network threads and the game
  // thread alike.
  std::mutex mutex;
  std::vector<WsEvent> pending;
  std::unordered_map<std::string, ix::WebSocket *> sockets;
};
