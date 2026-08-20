#include "Net/ws_proxy.hpp"

#include <cstdio>
#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <utility>

namespace {

// A browser client. Sends are routed back through the proxy rather than
// holding the socket directly, so a connection that closes on a network
// thread can never be written to after it has gone away.
class WsConnection final : public Connection {
public:
  WsConnection(WsProxy &proxy, std::string socketId)
      : proxy(proxy), socketId(std::move(socketId)) {}

  void send(const std::string &bytes, bool) override {
    proxy.sendTo(socketId, bytes);
  }

private:
  WsProxy &proxy;
  std::string socketId;
};

} // namespace

WsProxy::WsProxy(int port) {
  server = std::make_unique<ix::WebSocketServer>(port);

  server->setOnClientMessageCallback(
      [this](std::shared_ptr<ix::ConnectionState> state, ix::WebSocket &socket,
             const ix::WebSocketMessagePtr &msg) {
        const std::string id = state->getId();

        switch (msg->type) {
        case ix::WebSocketMessageType::Open: {
          WsEvent event;
          event.kind       = WsEvent::Kind::Connect;
          event.socketId   = id;
          event.connection = std::make_unique<WsConnection>(*this, id);

          std::lock_guard<std::mutex> lock(mutex);
          sockets[id] = &socket;
          pending.push_back(std::move(event));
          break;
        }

        case ix::WebSocketMessageType::Message: {
          // The protocol is cereal binary; text frames aren't ours.
          if (!msg->binary) {
            break;
          }

          WsEvent event;
          event.kind     = WsEvent::Kind::Message;
          event.socketId = id;
          event.data     = msg->str;

          std::lock_guard<std::mutex> lock(mutex);
          pending.push_back(std::move(event));
          break;
        }

        case ix::WebSocketMessageType::Close:
        case ix::WebSocketMessageType::Error: {
          WsEvent event;
          event.kind     = WsEvent::Kind::Disconnect;
          event.socketId = id;

          std::lock_guard<std::mutex> lock(mutex);
          sockets.erase(id);
          pending.push_back(std::move(event));
          break;
        }

        default:
          break;
        }
      });

  auto result = server->listen();
  if (!result.first) {
    std::fprintf(stderr, "[ws] failed to listen on port %d: %s\n", port,
                 result.second.c_str());
    return;
  }

  server->start();
  listening = true;
  std::printf("[ws] WebSocket proxy listening on port %d\n", port);
}

WsProxy::~WsProxy() {
  if (server && listening) {
    server->stop();
  }
}

std::vector<WsEvent> WsProxy::drain() {
  std::lock_guard<std::mutex> lock(mutex);
  std::vector<WsEvent> events;
  events.swap(pending);
  return events;
}

void WsProxy::sendTo(const std::string &socketId, const std::string &bytes) {
  std::lock_guard<std::mutex> lock(mutex);
  auto it = sockets.find(socketId);
  if (it != sockets.end()) {
    it->second->sendBinary(bytes);
  }
}
