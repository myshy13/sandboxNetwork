#pragma once

#include "Models/Object.hpp"
#include "Net/connection.hpp"
#include "Net/ws_proxy.hpp"
#include "Server/terrain.hpp"
#include "env.hpp"

#include <enet/enet.h>
#include <future>
#include <memory>
#include <optional>
#include <raylib.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Bullet {
  Vector3 pos;
  Vector3 vel;
  int playerId;
  int bulletId;

  float deathCountdown = env::BULLET_LIFETIME;
};

struct Player {
  int id;
  Vector3 pos;
  float pitch{0.0f};
  float yaw{0.0f};
  int health{env::PLAYER_MAX_HEALTH};
  std::optional<std::string> displayName;
};

struct ClientView {
  std::unordered_set<int64_t> loaded;
  int radius = env::DEFAULT_VIEW_RADIUS;
};

class Server {
  const std::string savePath;
  const int saveTime;
  int port{env::PORT};
  ENetHost *host;

  std::unique_ptr<WsProxy> wsProxy;

  // Every client, ENet and WebSocket alike, keyed by player id.
  std::unordered_map<int, std::unique_ptr<Connection>> connections;
  std::unordered_map<int, ClientView> views;
  std::unordered_map<std::string, int> wsPlayerIds;

  std::unordered_map<int, int> kills;

  int nextClientId{1};
  int nextBulletId{1};
  int nextObjectId{1};
  std::vector<Player> players{};
  std::vector<Object> objects{};
  // Grid cell -> index into `objects`; blocks are one per cell, so bullets test
  // a few cells, not every block.
  std::unordered_map<int64_t, int> occupiedCells;
  std::unordered_map<int64_t, std::vector<int>> chunkBlocks;
  std::vector<Bullet> bullets;

  std::unordered_set<int64_t> generatedChunks;

  Terrain terrain;

  float saveCountdownTime = saveTime;
  float saveCountdown{saveCountdownTime};

  // ==== World saving ==== //
  // Periodic saves write on another thread; `saving` is that write in flight.
  std::future<bool> saving;
  // Set by every block change, so an unchanged world isn't rewritten.
  bool worldChanged{false};
  void saveWorldAsync();

  Player *findPlayer(int id) {
    for (auto &p : players) {
      if (p.id == id)
        return &p;
    }
    return nullptr;
  }
  void deletePlayer(int id);
  std::optional<Bullet> createBullet(int playerId, Vector3 origin, Vector3 dir);

  // ==== transport-agnostic game logic ==== //
  int handleConnect(std::unique_ptr<Connection> connection);
  void handleDisconnect(int playerId);
  void handleReceive(int playerId, const std::string &data);
  void tick(float dt);

  void sendTo(int playerId, const std::string &bytes, bool reliable);
  void broadcast(const std::string &bytes, bool reliable);
  // Sends to every player who currently holds chunk `key` (see ClientView::loaded).
  void broadcastToChunk(int64_t key, const std::string &bytes, bool reliable);

  // ==== transport plumbing ==== //
  void pumpEnet();
  void pumpWebSockets();

  // ==== Blocks ==== //
  // Both keep `objects`, `occupiedCells` and `chunkBlocks` in sync (removal is swap-and-pop).
  void addBlock(const Object &block);
  void removeBlock(int index);
  // Records objects[i] in occupiedCells and in its chunk's list.
  void indexBlock(int i);
  // Sends chunk (cx, cz) to one player as a ChunkData, empty chunks included.
  void sendChunk(int playerId, int cx, int cz);
  // Index of the nearest block the segment from -> to passes through, or -1.
  int findBlockHit(Vector3 from, Vector3 to) const;

  // Interest management: unloads chunks that are too far from the player and
  // sends the nearest missing ones, at most env::CHUNKS_PER_TICK per call.
  void updateView(const Player &p);

  void loadWorld();
  // Blocks sharing a grid cell; returns how many are duplicates. 0 is a healthy world.
  int checkOverlaps() const;

  // ==== World generation ==== //
  void generateWorld();
  // Builds chunk (cx, cz) from `terrain` and adds its blocks via addBlock.
  void generateChunk(int cx, int cz);
  void ensureChunk(int cx, int cz);

public:
  void saveWorld();
  void poll();
  // wsPort of 0 leaves the browser proxy switched off.
  explicit Server(int wsPort = 0, const std::string savePath = "save.bin",
                  const int saveTime = 30, uint32_t seed = 0);
  ~Server();

  // ==== static consts ==== //
};
