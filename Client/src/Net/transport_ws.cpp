// Web transport: WebSocket via the browser, through the server's --ws-port
// proxy. Browsers cannot open raw UDP sockets, so ENet is not an option here.
// Compiled only for Emscripten builds - see Client/CMakeLists.txt.

#include "Net/transport.hpp"
#include "env.hpp"

#include <cstdio>
#include <deque>
#include <emscripten/websocket.h>

namespace {

class WebSocketTransport final : public Transport {
public:
  ~WebSocketTransport() override {
    if (socket > 0) {
      emscripten_websocket_close(socket, 1000, "client shutting down");
    }
  }

  void connect(const std::string &hostName, int port) override {
    if (!emscripten_websocket_is_supported()) {
      std::fprintf(stderr, "[net] browser has no WebSocket support\n");
      return;
    }

    // Held as a member: emscripten_websocket_new() does not copy the url,
    // so it has to outlive the call.
#ifdef SERVER_WSS
    url = "wss://" + hostName + ":" + std::to_string(port);
#else
    url = "ws://" + hostName + ":" + std::to_string(port);
#endif

    EmscriptenWebSocketCreateAttributes attrs;
    emscripten_websocket_init_create_attributes(&attrs);
    attrs.url = url.c_str();

    socket = emscripten_websocket_new(&attrs);
    if (socket <= 0) {
      std::fprintf(stderr, "[net] failed to create WebSocket\n");
      return;
    }

    emscripten_websocket_set_onopen_callback(socket, this, onOpen);
    emscripten_websocket_set_onclose_callback(socket, this, onClose);
    emscripten_websocket_set_onerror_callback(socket, this, onError);
    emscripten_websocket_set_onmessage_callback(socket, this, onMessage);
  }

  bool isConnected() const override { return connected; }

  // WebSocket rides on TCP, so every message is already reliable and ordered.
  // The flag exists for interface parity with ENet and is ignored here.
  void send(const std::string &bytes, bool) override {
    if (!connected) {
      return;
    }
    emscripten_websocket_send_binary(socket, const_cast<char *>(bytes.data()),
                                     static_cast<uint32_t>(bytes.size()));
  }

  std::optional<std::string> receive() override {
    if (inbox.empty()) {
      return std::nullopt;
    }
    std::string message = std::move(inbox.front());
    inbox.pop_front();
    return message;
  }

private:
  // Callbacks fire on the browser's main thread between frames, so they can
  // touch our state directly - no locking needed.
  static EM_BOOL onOpen(int, const EmscriptenWebSocketOpenEvent *,
                        void *userData) {
    static_cast<WebSocketTransport *>(userData)->connected = true;
    return EM_TRUE;
  }

  static EM_BOOL onClose(int, const EmscriptenWebSocketCloseEvent *,
                         void *userData) {
    static_cast<WebSocketTransport *>(userData)->connected = false;
    return EM_TRUE;
  }

  static EM_BOOL onError(int, const EmscriptenWebSocketErrorEvent *,
                         void *userData) {
    std::fprintf(stderr, "[net] WebSocket error\n");
    static_cast<WebSocketTransport *>(userData)->connected = false;
    return EM_TRUE;
  }

  static EM_BOOL onMessage(int, const EmscriptenWebSocketMessageEvent *event,
                           void *userData) {
    auto *self = static_cast<WebSocketTransport *>(userData);
    // The protocol is cereal binary; text frames aren't ours.
    if (!event->isText) {
      self->inbox.emplace_back(reinterpret_cast<const char *>(event->data),
                               event->numBytes);
    }
    return EM_TRUE;
  }

  EMSCRIPTEN_WEBSOCKET_T socket{0};
  std::string url;
  std::deque<std::string> inbox;
  bool connected{false};
};

} // namespace

std::unique_ptr<Transport> makeTransport() {
  return std::make_unique<WebSocketTransport>();
}
