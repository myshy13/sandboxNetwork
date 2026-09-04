// Native transport: ENet over UDP.
// Compiled only for native builds - see Client/CMakeLists.txt.

#include "Net/transport.hpp"

#include <cstdio>
#include <enet/enet.h>

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

} // namespace

std::unique_ptr<Transport> makeTransport() {
  return std::make_unique<EnetTransport>();
}
