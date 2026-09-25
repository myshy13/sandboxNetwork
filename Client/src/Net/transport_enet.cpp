// Native transport: ENet over UDP.
// Compiled only for native builds - see Client/CMakeLists.txt.

#include "Net/transport.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <enet/enet.h>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace {

class EnetTransport final : public Transport {
public:
  EnetTransport() {
    if (enet_initialize() != 0) {
      std::fprintf(stderr, "[net] failed to initialise ENet\n");
      return;
    }
    initialised = true;

    host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (host == nullptr) {
      std::fprintf(stderr, "[net] failed to create ENet client host\n");
    }
  }

  ~EnetTransport() override {
    if (host != nullptr) {
      enet_host_destroy(host);
    }
    if (initialised) {
      enet_deinitialize();
    }
  }

  void connect(const std::string &hostName, int port) override {
    if (host == nullptr) {
      return;
    }

    ENetAddress address;
    enet_address_set_host(&address, hostName.c_str());
    address.port = static_cast<enet_uint16>(port);

    peer = enet_host_connect(host, &address, 2, 0);
    if (peer == nullptr) {
      std::fprintf(stderr, "[net] no available peers for connection attempt\n");
    }
  }

  void disconnect() override {
    if (peer != nullptr) {
      enet_peer_disconnect_now(peer, 0);
      enet_host_flush(host);
      peer = nullptr;
    }
    connected = false;
  }

  bool isConnected() const override { return connected; }

  void send(const std::string &bytes, bool reliable) override {
    if (!connected || peer == nullptr) {
      return;
    }
    ENetPacket *packet =
        enet_packet_create(bytes.data(), bytes.size(),
                           reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(host);
  }

  std::optional<std::string> receive() override {
    if (host == nullptr) {
      return std::nullopt;
    }

    ENetEvent event;
    while (enet_host_service(host, &event, 0) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        connected = true;
        break;

      case ENET_EVENT_TYPE_DISCONNECT:
        connected = false;
        peer      = nullptr;
        break;

      case ENET_EVENT_TYPE_RECEIVE: {
        std::string data(reinterpret_cast<char *>(event.packet->data),
                         event.packet->dataLength);
        enet_packet_destroy(event.packet);
        return data;
      }

      case ENET_EVENT_TYPE_NONE:
        break;
      }
    }
    return std::nullopt;
  }

private:
  ENetHost *host{nullptr};
  ENetPeer *peer{nullptr};
  bool initialised{false};
  bool connected{false};
};

// ==== network thread ==== //
// Runs `inner` (ENet isn't thread-safe) on its own thread; the game thread only
// touches the mutex-guarded queues, so a slow frame never stalls the socket.
class ThreadedTransport final : public Transport {
public:
  explicit ThreadedTransport(std::unique_ptr<Transport> transport)
      : inner(std::move(transport)), worker([this] { run(); }) {}

  ~ThreadedTransport() override {
    running = false;
    worker.join();
    inner->disconnect(); // worker is gone, so the game thread may touch inner again
  }

  void connect(const std::string &host, int port) override {
    std::lock_guard lock(mutex);
    connectRequest = {host, port};
  }

  void disconnect() override {
    std::lock_guard lock(mutex);
    disconnectRequested = true;
    connected           = false;
  }

  bool isConnected() const override { return connected; }

  void send(const std::string &bytes, bool reliable) override {
    std::lock_guard lock(mutex);
    outbound.push_back({bytes, reliable});
  }

  std::optional<std::string> receive() override {
    std::lock_guard lock(mutex);
    if (inbound.empty()) {
      return std::nullopt;
    }
    std::string data = std::move(inbound.front());
    inbound.pop_front();
    return data;
  }

private:
  struct Outgoing {
    std::string bytes;
    bool reliable;
  };

  void run() {
    using namespace std::chrono_literals;
    while (running) {
      // Take everything the game thread queued in one short lock.
      std::optional<std::pair<std::string, int>> toConnect;
      std::deque<Outgoing> toSend;
      bool toDisconnect;
      {
        std::lock_guard lock(mutex);
        toConnect    = std::exchange(connectRequest, std::nullopt);
        toDisconnect = std::exchange(disconnectRequested, false);
        toSend       = std::exchange(outbound, {});
      }

      if (toConnect) {
        inner->connect(toConnect->first, toConnect->second);
      }
      for (const Outgoing &o : toSend) {
        inner->send(o.bytes, o.reliable);
      }
      if (toDisconnect) {
        inner->disconnect();
      }

      bool received = false;
      while (auto data = inner->receive()) {
        received = true;
        std::lock_guard lock(mutex);
        inbound.push_back(std::move(*data));
      }
      if (!toDisconnect) {
        connected = inner->isConnected();
      }

      // inner->receive() doesn't block, so idle briefly instead of spinning a core.
      if (!received && toSend.empty()) {
        std::this_thread::sleep_for(1ms);
      }
    }
  }

  std::unique_ptr<Transport> inner;
  std::mutex mutex; // guards everything below except the atomics
  std::deque<std::string> inbound;
  std::deque<Outgoing> outbound;
  std::optional<std::pair<std::string, int>> connectRequest;
  bool disconnectRequested{false};
  std::atomic<bool> connected{false};
  std::atomic<bool> running{true};
  std::thread worker; // last, so every member above exists before run() starts
};

} // namespace

std::unique_ptr<Transport> makeTransport() {
  return std::make_unique<ThreadedTransport>(std::make_unique<EnetTransport>());
}
