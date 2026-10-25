#pragma once

#include "Auth/tickets.hpp"
#include "Fluid/fluidSim.hpp"
#include "Models/Object.hpp"
#include "Net/connection.hpp"
#include "Net/ws_proxy.hpp"
#include "Terrain/terrain.hpp"
#include "env.hpp"

#include <chrono>
#include <cstdint>
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
  float shotBudget{
      env::SHOT_BURST}; // refills one shot per SHOT_INTERVAL, up to SHOT_BURST
  // Fall tracking, derived from position updates (see Server::updateFall).
  bool airborne{false};
  float peakY{0.0f}; // highest feet height since the player last stood or swam

  bool handshaken{false};
  std::optional<uint32_t> accountId; // guest if empty
  bool isAdmin{false};
  uint32_t ticketExp{0};

  // Call whenever pos is set by the server (spawn, respawn): a teleport isn't a
  // fall.
  void resetFall() {
    airborne = false;
    peakY = pos.y;
  }
};

struct ClientView {
  std::unordered_set<int64_t> loaded;
  int radius = env::DEFAULT_VIEW_RADIUS;
};

class Server {
  bool freezeTime{false};
  bool allowGuests{false};
  const std::string savePath;
  const int maxPlayers = env::DEFAULT_MAX_PLAYERS;
  const int saveTime;
  float timeOfDay{env::DAY_DEFAULT_TIME};
  int port{env::PORT};
  ENetHost *host;

  std::unique_ptr<WsProxy> wsProxy;

  // Every client, ENet and WebSocket alike, keyed by player id.
  std::unordered_map<int, std::unique_ptr<Connection>> connections;
  std::unordered_map<int, ClientView> views;
  std::unordered_map<std::string, int> wsPlayerIds;

  std::optional<Tickets> tickets;
  std::unordered_map<uint32_t, int>
      accountToPlayer; // acountId -> player id. (not guests)
  std::unordered_map<uint32_t, uint64_t> revokedUntil;

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
  std::unordered_set<int64_t> dirtyChunks; // modified chunks since last save

  Terrain terrain;
  FluidSim fluidSim;

  float saveCountdownTime = saveTime;
  float saveCountdown{saveCountdownTime};
  float timeBroadcastCountdown{env::TIME_BROADCAST_INTERVAL};
  float ticketCheckCountdown{env::EXPIRY_CHECK_INTERVAL};
  float statusLogCountdown{env::SERVER_STATUS_INTERVAL};

  // ==== World saving ==== //
  // Periodic saves write on another thread; `saving` is that write in flight.
  std::future<std::unordered_set<int64_t>> saving;
  void saveWorldAsync();

  FILE *console;

  // ==== status log ==== //
  // Counters cover one status interval; logStatus() prints then resets them.
  std::chrono::steady_clock::time_point startTime{
      std::chrono::steady_clock::now()};
  int statusTicks{0};
  float statusDtSum{0.0f};
  float statusMaxDt{0.0f};
  int statusBulletsFired{0};
  int statusKills{0};
  int statusReceived{0};
  int statusSent{0};
  void logStatus();

  Player *findPlayer(int id) {
    for (auto &p : players) {
      if (p.id == id)
        return &p;
    }
    return nullptr;
  }
  void deletePlayer(int id);
  std::optional<Bullet> createBullet(Player *p, Vector3 origin, Vector3 dir);

  // ==== transport-agnostic game logic ==== //
  int handleConnect(std::unique_ptr<Connection> connection);
  void handleDisconnect(int playerId);
  void handleReceive(int playerId, const std::string &data);
  void tick(float dt);

  void sendTo(int playerId, const std::string &bytes, bool reliable);
  void broadcast(const std::string &bytes, bool reliable);
  // Asks the transport to close this player's connection once queued sends are
  // out.
  void disconnectPlayer(int playerId);
  // Sends to every player who currently holds chunk `key` (see
  // ClientView::loaded).
  void broadcastToChunk(int64_t key, const std::string &bytes, bool reliable);

  // ==== transport plumbing ==== //
  void pumpEnet();
  void pumpWebSockets();

  // ==== Blocks ==== //
  // Both keep `objects`, `occupiedCells` and `chunkBlocks` in sync (removal is
  // swap-and-pop). markDirty is false for generated/loaded baseline content,
  // which isn't an edit and shouldn't cause an unchanged chunk to be rewritten
  // to disk.
  void addBlock(const Object &block, bool markDirty = true,
                bool activate = true);
  void removeBlock(int index);
  // Sets a water cell's flow level and notifies every player holding its chunk.
  // No-op off a water cell.
  void setWaterLevel(Vector3 pos, uint8_t level);
  // Records objects[i] in occupiedCells and in its chunk's list.
  void indexBlock(int i);
  // Sends chunk (cx, cz) to one player as a ChunkData, empty chunks included.
  void sendChunk(int playerId, int cx, int cz);
  // Index of the nearest block the segment from -> to passes through, or -1.
  int findBlockHit(Vector3 from, Vector3 to) const;
  // True if any block under the player's footprint (every cell the box
  // overlaps in x and z) at height y satisfies pred.
  bool footprintHas(Vector3 pos, float y, bool (*pred)(const Object &)) const;
  // Tracks the highest point of a fall from position updates and applies damage
  // when the player lands on something solid. Water cancels the fall.
  void updateFall(Player *player);
  // The one place health drops: broadcasts PlayerHit and handles death and
  // respawn. attackerId < 0 means no attacker (a fall), so no kill credit.
  void damagePlayer(Player &p, int amount, int attackerId);
  // Interest management: unloads chunks that are too far from the player and
  // sends the nearest missing ones, at most env::CHUNKS_PER_TICK per call.
  void updateView(const Player &p);

  // Blocks sharing a grid cell; returns how many are duplicates. 0 is a healthy
  // world.
  int checkOverlaps() const;

  // ==== World generation ==== //
  // Builds chunk (cx, cz) from `terrain` and adds its blocks via addBlock.
  void generateChunk(int cx, int cz);
  Vector3 randomSpawn() const;
  void ensureChunk(int cx, int cz);

public:
  void saveWorld();
  void poll();
  // wsPort of 0 leaves the browser proxy switched off.
  explicit Server(FILE *console, int wsPort = 0,
                  const std::string savePath = "save", const int saveTime = 30,
                  uint32_t seed = 0, int nextObjectId = 1,
                  int MAX_PLAYERS = env::DEFAULT_MAX_PLAYERS,
                  float timeOfDay = env::DAY_DEFAULT_TIME,
                  bool freezeTime = false, bool allowGuests = false,
                  const std::string &pubkeyPath = "");
  ~Server();

  // ==== static consts ==== //
};
