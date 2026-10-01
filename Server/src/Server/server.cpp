#include "Server/server.hpp"
#include "Fluid/fluidSim.hpp"
#include "Models/Object.hpp"
#include "Protocol/protocol.hpp"
#include "Terrain/chunk.hpp"
#include "Terrain/structures.hpp"
#include "enet/enet.h"
#include "env.hpp"
#include "raylib.h"
#include "sharedEnv.hpp"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <future>
#include <hfs/hfs_format.h>
#include <optional>
#include <raymath.h>
#include <string>
#include <unordered_map>
#include <vector>

constexpr Vector3 PLAYER_SCALE = SHARED_PLAYER_SCALE;

namespace {
// Client-sent positions go through this: a NaN or infinity would make every
// floorf-to-int cell lookup undefined.
bool isFinite(Vector3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// A player-claimed position the server will act on: real numbers, inside the
// range cell keys can hold.
bool isValidPosition(Vector3 v) {
  return isFinite(v) && std::abs(v.x) < env::WORLD_LIMIT &&
         std::abs(v.y) < env::WORLD_LIMIT && std::abs(v.z) < env::WORLD_LIMIT;
}

// An ENet client. The WebSocket equivalent lives in Net/ws_proxy.cpp.
class EnetConnection final : public Connection {
public:
  explicit EnetConnection(ENetPeer *peer) : peer(peer) {}

  void send(const std::string &bytes, bool reliable) override {
    if (peer == nullptr) {
      return;
    }
    ENetPacket *packet = enet_packet_create(
        bytes.data(), bytes.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    enet_peer_send(peer, 0, packet);
  }

private:
  ENetPeer *peer;
};

} // namespace

// ==== connection setup ==== //

Server::Server(int wsPort, std::string savePath, int saveTime, uint32_t seed,
               int nextObjectId, int maxPlayers)
    : savePath(std::move(savePath)), maxPlayers(maxPlayers), saveTime(saveTime),
      nextObjectId(nextObjectId), terrain(seed) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (enet_initialize() != 0) {
    std::fprintf(stderr, "Failed to initialize ENet\n");
    std::exit(EXIT_FAILURE);
  }
  std::printf("ENet initialized\n");

  // Bind to the listen port - a null address makes a client-only host that
  // never accepts connections.
  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = static_cast<enet_uint16>(port);
  host = enet_host_create(&address, 32, 2, 0, 0);
  if (host == nullptr) {
    std::fprintf(stderr, "Failed to create ENet server host\n");
    std::exit(EXIT_FAILURE);
  }

  if (wsPort > 0) {
    wsProxy = std::make_unique<WsProxy>(wsPort);
    if (!wsProxy->isListening()) {
      wsProxy.reset(); // it already explained itself on stderr
    }
  }

  checkOverlaps();
}

Server::~Server() {
  saveWorld();
  connections.clear(); // before the proxy and host they point into
  if (host != nullptr) {
    enet_host_destroy(host);
  }
  enet_deinitialize();
}

// ==== block grid ==== //
#include "./blockHelpers.hpp"

// The key of the chunk a world position falls in.
static int64_t chunkKeyAt(Vector3 pos) {
  // Divide and floor while still a float; a plain (int) cast rounds toward
  // zero, which is wrong below 0.
  return chunkKey((int)floorf(pos.x / CHUNK_SIZE),
                  (int)floorf(pos.z / CHUNK_SIZE));
}

// ==== World saving ==== //
// Blocks until any background save has finished, then writes synchronously.
// Used at shutdown, where the write has to be done before we exit.
void Server::saveWorld() {
  if (saving.valid()) {
    for (int64_t failedKey :
         saving.get()) // get() waits for the background write to finish
      dirtyChunks.insert(failedKey); // retry ones that failed last time
  }
  if (dirtyChunks.empty())
    return;

  std::unordered_map<int64_t, SavedChunk> snapshot;
  for (int64_t key : dirtyChunks) {
    std::vector<Object> chunkObjects;
    if (auto it = chunkBlocks.find(key);
        it != chunkBlocks.end()) { // find, not []: no empty lists
      for (int b : it->second) {
        chunkObjects.push_back(objects[b]);
      }
    }
    SavedChunk chunk;
    chunk.blocks = std::move(chunkObjects);
    snapshot[key] = chunk;
  }
  dirtyChunks.clear(); // clear right after the copy, not after the write

  SaveMeta meta{env::saveFormatVersion, env::terrainVersion, terrain.seed(),
                nextObjectId};
  writeMetaFile(metaFilePath(savePath), meta);

  for (auto &[key, chunk] : snapshot) {
    auto [cx, cz] = chunkCoords(key);
    if (!writeChunkFile(chunkFilePath(savePath, cx, cz), chunk))
      std::printf("ERR: chunk %d, %d was not saved; its edits are lost\n", cx,
                  cz); // no later save to retry in
  }
}

// Snapshots the world here (a memory copy), then writes it on another thread
// so the tick never waits on the disk.
void Server::saveWorldAsync() {
  if (saving.valid() &&
      saving.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return; // still writing the last batch, try again next period
  if (saving.valid()) {
    for (int64_t failedKey : saving.get())
      dirtyChunks.insert(failedKey); // retry ones that failed last time
  }
  if (dirtyChunks.empty())
    return;

  std::unordered_map<int64_t, SavedChunk> snapshot;
  for (int64_t key : dirtyChunks) {
    std::vector<Object> chunkObjects;
    if (auto it = chunkBlocks.find(key);
        it != chunkBlocks.end()) { // find, not []: no empty lists
      for (int b : it->second) {
        chunkObjects.push_back(objects[b]);
      }
    }
    SavedChunk chunk;
    chunk.blocks = std::move(chunkObjects);
    snapshot[key] = chunk;
  }
  dirtyChunks.clear(); // clear right after the copy, not after the write

  SaveMeta meta{env::saveFormatVersion, env::terrainVersion, terrain.seed(),
                nextObjectId};
  writeMetaFile(metaFilePath(savePath), meta);

  saving = std::async(
      std::launch::async, [path = savePath, snapshot = std::move(snapshot)] {
        std::unordered_set<int64_t> failed;
        for (auto &[key, chunk] : snapshot) {
          auto [cx, cz] = chunkCoords(key);
          if (!writeChunkFile(chunkFilePath(path, cx, cz), chunk))
            failed.insert(key);
        }
        return failed;
      });
}

// Diagnostic: occupiedCells keeps one index per cell, so duplicates hide in
// `objects`.
int Server::checkOverlaps() const {
  std::unordered_map<int64_t, int> counts;
  counts.reserve(objects.size());
  for (const Object &o : objects) {
    counts[blockKey(o.getTransform().pos)]++;
  }

  int cells = 0;
  int extra = 0;
  for (const auto &[key, count] : counts) {
    if (count > 1) {
      cells++;
      extra += count - 1;
    }
  }
  if (cells > 0) {
    std::fprintf(
        stderr,
        "overlapping blocks: %d cells hold 2+ blocks (%d extra of %zu)\n",
        cells, extra, objects.size());
  } else {
    std::printf("no overlapping blocks (%zu objects)\n", objects.size());
  }
  return extra;
}

void Server::ensureChunk(int cx, int cz) {
  int64_t key = chunkKey(cx, cz);
  if (generatedChunks.contains(key))
    return;

  auto saved = readChunkFile(chunkFilePath(savePath, cx, cz));
  if (saved.has_value()) {
    // Wake the surface of every body of water (water with no water above it),
    // including mid-flow water; what lies under a surface is already settled.
    std::unordered_set<int64_t> waterCells;
    for (const Object &block : saved->blocks) {
      if (block.getType() == BlockType::Water)
        waterCells.insert(blockKey(block.getTransform().pos));
    }
    for (const Object &block : saved->blocks) {
      const Vector3 above =
          Vector3Add(block.getTransform().pos, {0, BLOCK_SIZE, 0});
      const bool surface = block.getType() == BlockType::Water &&
                           !waterCells.contains(blockKey(above));
      addBlock(block, false, surface); // loaded, not a new edit
    }
  } else {
    generateChunk(cx, cz);
  }
  generatedChunks.insert(key);
};

// Random column, standing on the tallest cell the player's box overlaps.
Vector3 Server::randomSpawn() const {
  Vector3 spawn;
  spawn.x = rand() % 200 - 100;
  spawn.z = rand() % 200 - 100;

  // floorf, not an int cast: the cast rounds toward zero, so negative x/z pick
  // the wrong cell.
  int minX = (int)floorf((spawn.x - PLAYER_SCALE.x / 2) / BLOCK_SIZE);
  int maxX = (int)floorf((spawn.x + PLAYER_SCALE.x / 2) / BLOCK_SIZE);
  int minZ = (int)floorf((spawn.z - PLAYER_SCALE.z / 2) / BLOCK_SIZE);
  int maxZ = (int)floorf((spawn.z + PLAYER_SCALE.z / 2) / BLOCK_SIZE);

  int height = 0;
  for (int cellX = minX; cellX <= maxX; cellX++)
    for (int cellZ = minZ; cellZ <= maxZ; cellZ++)
      height = std::max(height, terrain.heightAt(cellX, cellZ));

  spawn.y = (height + 1) * BLOCK_SIZE + BLOCK_SIZE / 2 +
            PLAYER_SCALE.y; // a bit above
  return spawn;
}

void Server::generateChunk(int cx, int cz) {
  // A large odd offset so a below-layer's damage roll doesn't reuse another
  // real column's hash by coincidence.
  constexpr int DAMAGE_OFFSET = 999983;

  for (int cellX = cx * 16; cellX < cx * 16 + 16; cellX++) {
    for (int cellZ = cz * 16; cellZ < cz * 16 + 16; cellZ++) {
      int height = terrain.heightAt(cellX, cellZ);
      if (height >= env::WATER_HEIGHT) {
        float blockX = cellX * blockSize.x + (blockSize.x / 2);
        float blockY = height * blockSize.y + (blockSize.y / 2);
        float blockZ = cellZ * blockSize.z + (blockSize.z / 2);

        Object top(nextObjectId,
                   ObjectTransform{{blockX, blockY, blockZ}, blockSize}, GREEN);
        int topDamage = terrain.hash(cellX, cellZ) % 3;
        for (int i = 0; i < topDamage; i++) {
          top.damage();
        }
        addBlock(top, false); // generated, not a new edit
        nextObjectId++;

        for (int i = height; i > 0; i--) {
          blockY -= blockSize.y;
          Object below(nextObjectId,
                       ObjectTransform{{blockX, blockY, blockZ}, blockSize},
                       BROWN);
          int belowDamage = terrain.hash(cellX + i * DAMAGE_OFFSET, cellZ) % 3;
          for (int d = 0; d < belowDamage; d++) {
            below.damage();
          }
          addBlock(below, false); // generated, not a new edit
          nextObjectId++;
        }

      } else {
        // water
        float blockX = cellX * blockSize.x + (blockSize.x / 2);
        float blockY = (env::WATER_HEIGHT + 1) * blockSize.y + blockSize.y / 2;
        float blockZ = cellZ * blockSize.z + (blockSize.z / 2);

        for (int i = height + 1; i > 0; i--) {
          blockY -= blockSize.y;
          Object o(nextObjectId,
                   ObjectTransform{{blockX, blockY, blockZ}, blockSize}, BLUE,
                   BlockType::Water);
          // Only the topmost layer is a live source; the rest are already at
          // rest against the floor and each other, so they don't need FluidSim
          // to touch them - seeding every layer floods activeCells on every
          // chunk load and stalls the client on chunk-mesh rebuilds.
          bool isTopLayer = (i == height + 1);
          if (!isTopLayer)
            o.setLevel(1); // fed from above, not a source: drains if cut off
          addBlock(o, false, isTopLayer); // generated, not a new edit
          nextObjectId++;
        }
        blockY -= blockSize.y;
        Object o(nextObjectId,
                 ObjectTransform{{blockX, blockY, blockZ}, blockSize}, BROWN);
        int damage = terrain.hash(cellX + height - 1, cellZ) % 2;
        for (int d = 0; d < damage; d++) {
          o.damage();
        }
        addBlock(o, false); // generated, not a new edit
        nextObjectId++;
      }
    }
  }

  // Trees: a separate pass over a margin around this chunk, since a trunk
  // near an edge can have canopy blocks landing in the neighboring chunk -
  // every chunk recomputes the whole tree from the trunk's column and keeps
  // only the blocks that land inside itself.
  constexpr int CANOPY_MARGIN = 2;
  auto chunkOfCell = [](int cell) {
    return cell >= 0 ? cell / 16 : (cell - 15) / 16;
  };
  for (int cellX = cx * 16 - CANOPY_MARGIN;
       cellX < cx * 16 + 16 + CANOPY_MARGIN; cellX++) {
    for (int cellZ = cz * 16 - CANOPY_MARGIN;
         cellZ < cz * 16 + 16 + CANOPY_MARGIN; cellZ++) {
      int height = terrain.heightAt(cellX, cellZ);
      if (height < env::WATER_HEIGHT || !terrain.hasTree(cellX, cellZ))
        continue;

      float blockX = cellX * blockSize.x + (blockSize.x / 2);
      float blockZ = cellZ * blockSize.z + (blockSize.z / 2);
      Vector3 base = {blockX, height * blockSize.y + (blockSize.y / 2), blockZ};

      for (auto &block : TREE_SHAPE) {
        if (chunkOfCell(cellX + block.dx) != cx ||
            chunkOfCell(cellZ + block.dz) != cz)
          continue; // belongs to a neighboring chunk, which places it itself

        Object b(
            nextObjectId,
            ObjectTransform{Vector3Add(base, {(float)block.dx * BLOCK_SIZE,
                                              (float)block.dy * BLOCK_SIZE,
                                              (float)block.dz * BLOCK_SIZE}),
                            blockSize},
            block.color);
        for (int i = terrain.hash(cellX, cellZ * block.dy) % 3; i > 0; i--) {
          b.damage();
        }
        addBlock(b, false); // generated, not a new edit
        nextObjectId++;
      }
    }
  }
}

// ==== sending ==== //
void Server::sendTo(int playerId, const std::string &bytes, bool reliable) {
  auto it = connections.find(playerId);
  if (it != connections.end()) {
    it->second->send(bytes, reliable);
  }
}

void Server::broadcast(const std::string &bytes, bool reliable) {
  for (auto &entry : connections) {
    entry.second->send(bytes, reliable);
  }
}

void Server::broadcastToChunk(int64_t key, const std::string &bytes,
                              bool reliable) {
  for (const auto &[id, view] : views) {
    if (view.loaded.contains(key)) {
      sendTo(id, bytes, reliable);
    }
  }
}

// ==== player bookkeeping ==== //
void Server::deletePlayer(int id) {
  auto it = std::find_if(players.begin(), players.end(),
                         [id](const Player &p) { return p.id == id; });
  if (it != players.end()) {
    players.erase(it);
  }
}

// ==== Bullet handling ==== //
std::optional<Bullet> Server::createBullet(int playerId, Vector3 origin,
                                           Vector3 dir) {
  Player *player = findPlayer(playerId);
  if (player == nullptr) {
    return std::nullopt;
  }
  // The client picks the aim ray (see CreateBullet), but it must start at the
  // shooter, and only so often.
  if (!isFinite(origin) || !isFinite(dir) || Vector3Length(dir) == 0.0f ||
      Vector3Distance(origin, player->pos) > env::MAX_MUZZLE_OFFSET ||
      player->shotBudget < 1.0f) {
    return std::nullopt;
  }
  player->shotBudget -= 1.0f;

  Vector3 forward = Vector3Normalize(dir);

  Bullet bullet;
  bullet.bulletId = nextBulletId;
  nextBulletId++;
  bullet.playerId = player->id;
  bullet.pos = Vector3Add(origin, Vector3Scale(forward, env::MUZZLE_DISTANCE));
  bullet.vel = Vector3Scale(forward, env::BULLET_SPEED);

  bullets.push_back(bullet);
  return bullet;
};

// ==== event handlers ==== //
int Server::handleConnect(std::unique_ptr<Connection> connection) {
  const int id = nextClientId;
  nextClientId++;
  std::printf("Client %d connected\n", id);

  connections[id] = std::move(connection);

  Player newPlayer;
  newPlayer.id = id;
  Vector3 spawnPos = randomSpawn();
  newPlayer.pos = spawnPos;
  players.push_back(newPlayer);
  views.emplace(id, ClientView{}); // now, so a SetViewRadius that arrives
                                   // before the first tick has a view to set

  if ((int)players.size() > maxPlayers) {
    sendTo(id,
           proto::pack(proto::Type::kick, proto::kick{id, "Too many players"}),
           true);
  }

  // handshake stuff
  sendTo(id, proto::pack(proto::Type::GivenId, proto::GivenId{id}), true);
  sendTo(id, proto::pack(proto::Type::Respawn, proto::Respawn{spawnPos}), true);
  // Names are only broadcast when set, so a newcomer needs everyone's current
  // one.
  for (const Player &p : players) {
    if (p.displayName.has_value()) {
      sendTo(id,
             proto::pack(proto::Type::SetName,
                         proto::SetName{*p.displayName, p.id}),
             true);
    }
  }
  return id;
}

void Server::handleDisconnect(int playerId) {
  std::printf("Client %d disconnected\n", playerId);
  deletePlayer(playerId);
  connections.erase(playerId);
  views.erase(playerId);

  broadcast(
      proto::pack(proto::Type::DeletePlayer, proto::DeletePlayer{playerId}),
      true);
}

void Server::handleReceive(int playerId, const std::string &data) {
  if (data.empty()) {
    return;
  }
  try {
    switch (proto::peekType(data)) {
    // ==== pos update handler ==== //
    case proto::Type::PlayerUpdate: {
      auto msg = proto::unpack<proto::PlayerUpdate>(data);
      msg.id = playerId; // trust the connection, not the payload
      if (!isValidPosition(msg.pos) || !std::isfinite(msg.pitch) ||
          !std::isfinite(msg.yaw)) {
        break;
      }

      Player *player = findPlayer(playerId);
      if (player != nullptr) {
        player->pos = msg.pos;
        player->pitch = msg.pitch;
        player->yaw = msg.yaw;
      }
      // ==== notify all peers ==== //
      // Unreliable on purpose: a dropped position is superseded a frame later.
      broadcast(proto::pack(proto::Type::PlayerUpdate, msg), false);
      break;
    }

    case proto::Type::CreateBullet: {
      auto msg = proto::unpack<proto::CreateBullet>(data);
      std::optional<Bullet> bullet =
          createBullet(playerId, msg.origin, msg.dir);
      if (bullet.has_value()) {
        proto::NewBullet packetMsg;
        packetMsg.bulletId = bullet->bulletId;
        packetMsg.playerId = bullet->playerId;
        packetMsg.pos = bullet->pos;
        packetMsg.vel = bullet->vel;
        broadcast(proto::pack(proto::Type::NewBullet, packetMsg), true);
      }
      break;
    }

    case proto::Type::ChatMessage: {
      auto msg = proto::unpack<proto::ChatMessage>(data);
      msg.id = playerId; // trust the connection, not the payload
      broadcast(proto::pack(proto::Type::ChatMessage, msg), true);
      break;
    }

    case proto::Type::SetName: {
      auto msg = proto::unpack<proto::SetName>(data);
      msg.id = playerId; // trust the connection, not the payload
      const bool taken =
          std::any_of(players.begin(), players.end(), [&](const Player &p) {
            return p.id != playerId && p.displayName == msg.name;
          });
      if (auto *player = findPlayer(playerId); player && !taken) {
        player->displayName = msg.name;
        broadcast(proto::pack(proto::Type::SetName, msg), true);
      }
      break;
    }

    case proto::Type::PlaceObject: {
      auto msg = proto::unpack<proto::PlaceObject>(data);
      const Vector3 sentPos = msg.object.getTransform().pos;
      const BlockType type = msg.object.getType();
      const Player *player = findPlayer(playerId);
      if (player == nullptr || !isValidPosition(sentPos) ||
          (type != BlockType::Solid && type != BlockType::Water)) {
        break;
      }
      // Only the type and colour are the client's choice: snap to the grid, no
      // further than the player can reach.
      const Vector3 pos = {(floorf(sentPos.x / BLOCK_SIZE) + 0.5f) * BLOCK_SIZE,
                           (floorf(sentPos.y / BLOCK_SIZE) + 0.5f) * BLOCK_SIZE,
                           (floorf(sentPos.z / BLOCK_SIZE) + 0.5f) *
                               BLOCK_SIZE};
      if (Vector3Distance(pos, player->pos) > env::PLACE_REACH) {
        break;
      }
      const int64_t chunk = chunkKeyAt(pos);
      auto view = views.find(playerId);
      if (view == views.end() || !view->second.loaded.contains(chunk)) {
        break; // a player can only edit a chunk they hold
      }
      if (auto occupant = occupiedCells.find(blockKey(pos));
          occupant != occupiedCells.end()) {
        if (objects[occupant->second].getType() != BlockType::Water) {
          break; // one block per cell, and only water can be placed over
        }
        broadcastToChunk(
            chunk,
            proto::pack(proto::Type::RemoveObject, proto::RemoveObject{pos}),
            true);
        removeBlock(occupant->second);
      }
      // A fresh block: default durability, source level, block size and a
      // server-owned id.
      const Object placed(nextObjectId++, ObjectTransform{pos, blockSize},
                          msg.object.getColor(), type);
      addBlock(placed);
      broadcastToChunk(
          chunk, proto::pack(proto::Type::NewObject, proto::NewObject{placed}),
          true); // reliable
      break;
    }

    case proto::Type::SetViewRadius: {
      auto msg = proto::unpack<proto::SetViewRadius>(data);
      // Identity comes from the connection; the radius is client-supplied, so
      // clamp it.
      if (auto it = views.find(playerId); it != views.end()) {
        it->second.radius = std::clamp(msg.radius, 2, env::MAX_VIEW_RADIUS);
      }
      break;
    }

    case proto::Type::clientHandshake: {
      auto msg = proto::unpack<proto::clientHandshake>(data);
      if (msg.ver != proto::PROTOCOL_VERSION) {
        auto kickBytes = proto::pack(
            proto::Type::kick,
            proto::kick{playerId,
                        "Mismatch Client version: " + std::to_string(msg.ver) +
                            ". Server version: " +
                            std::to_string(proto::PROTOCOL_VERSION)});
        sendTo(playerId, kickBytes, true);
      }
      break;
    }

    default:
      std::printf("Invalid request\n");
      break;
    }
  } catch (const std::exception &e) {
    std::printf("ERR: bad message (type %d) from player %d: %s\n",
                (int)proto::peekType(data), playerId, e.what());
  }
}

// ==== fixed-rate tick (bullet lifetime etc.) ==== //

bool SegmentIntersectsBox(Vector3 start, Vector3 end, BoundingBox box) {
  Vector3 dir = Vector3Subtract(end, start);
  float tMin = 0.0f;
  float tMax = 1.0f;

  auto clipAxis = [&](float s, float d, float boxMin, float boxMax) {
    if (fabsf(d) < 1e-6f) {
      return s >= boxMin && s <= boxMax;
    }
    float t1 = (boxMin - s) / d;
    float t2 = (boxMax - s) / d;
    if (t1 > t2) {
      std::swap(t1, t2);
    }
    tMin = std::max(tMin, t1);
    tMax = std::min(tMax, t2);
    return tMin <= tMax;
  };

  return clipAxis(start.x, dir.x, box.min.x, box.max.x) &&
         clipAxis(start.y, dir.y, box.min.y, box.max.y) &&
         clipAxis(start.z, dir.z, box.min.z, box.max.z);
}

// ==== blocks ==== //
void Server::indexBlock(int i) {
  const Vector3 &pos = objects[i].getTransform().pos;
  occupiedCells[blockKey(pos)] = i;
  chunkBlocks[chunkKeyAt(pos)].push_back(i);
}

void Server::sendChunk(int playerId, int cx, int cz) {
  proto::ChunkData msg{cx, cz, {}};
  // find, not []: [] would insert an empty list. An empty chunk is still
  // sent, so the client knows it has loaded.
  auto it = chunkBlocks.find(chunkKey(cx, cz));
  if (it != chunkBlocks.end()) {
    msg.blocks.reserve(it->second.size());
    for (int i : it->second) {
      msg.blocks.push_back(objects[i]); // a copy: the snapshot at this moment
    }
  }
  sendTo(playerId, proto::pack(proto::Type::ChunkData, msg), true);
}

void Server::updateView(const Player &p) {
  ClientView &view = views[p.id];
  // The player's chunk; floor while still a float, as in chunkKeyAt.
  const int pcx = (int)floorf(p.pos.x / CHUNK_SIZE);
  const int pcz = (int)floorf(p.pos.z / CHUNK_SIZE);

  // ==== unload ==== //
  std::vector<int64_t> toUnload;
  for (int64_t key : view.loaded) {
    auto [cx, cz] = chunkCoords(key);
    int dx = cx - pcx;
    int dz = cz - pcz;
    bool tooFar =
        std::abs(dx) > view.radius + 1 || std::abs(dz) > view.radius + 1;
    if (tooFar) {
      toUnload.push_back(key);
    }
  }
  for (int64_t key : toUnload) {
    auto [cx, cz] = chunkCoords(key);
    sendTo(p.id,
           proto::pack(proto::Type::ChunkUnload, proto::ChunkUnload{cx, cz}),
           true);
    view.loaded.erase(key);
  }

  // ==== load ==== //
  std::vector<std::pair<int, int>> toLoad;
  for (int cx = pcx - view.radius; cx <= pcx + view.radius; cx++) {
    for (int cz = pcz - view.radius; cz <= pcz + view.radius; cz++) {
      if (!view.loaded.contains(chunkKey(cx, cz))) {
        toLoad.push_back({cx, cz});
      }
    }
  }

  // Nearest first, so the chunks under the player arrive before the far ones.
  auto distSq = [&](const std::pair<int, int> &c) {
    int dx = c.first - pcx;
    int dz = c.second - pcz;
    return dx * dx + dz * dz;
  };
  std::sort(toLoad.begin(), toLoad.end(), [&](const auto &a, const auto &b) {
    return distSq(a) < distSq(b);
  });

  size_t count = std::min(toLoad.size(), (size_t)env::CHUNKS_PER_TICK);
  for (size_t i = 0; i < count; i++) {
    auto [cx, cz] = toLoad[i];
    ensureChunk(cx, cz);
    sendChunk(p.id, cx, cz);
    view.loaded.insert(chunkKey(cx, cz));
  }
}

void Server::addBlock(const Object &block, bool markDirty, bool activate) {
  objects.push_back(block);
  indexBlock((int)objects.size() - 1);
  if (markDirty) {
    dirtyChunks.insert(chunkKeyAt(block.getTransform().pos));
  }
  if (activate && block.getType() == BlockType::Water)
    fluidSim.markActive(blockKey(block.getTransform().pos));
}

void Server::setWaterLevel(Vector3 pos, uint8_t level) {
  auto occupant = occupiedCells.find(blockKey(pos));
  if (occupant == occupiedCells.end()) {
    if (!generatedChunks.contains(chunkKeyAt(pos)))
      return; // generateChunk would later build over it (or it'd save as a
              // terrain-less chunk)
    Object o(nextObjectId++, ObjectTransform{pos, blockSize}, BLUE,
             BlockType::Water);
    o.setLevel(level);
    addBlock(o);
    broadcastToChunk(chunkKeyAt(pos),
                     proto::pack(proto::Type::NewObject, proto::NewObject{o}),
                     true);
    return;
  }
  if (objects[occupant->second].getType() != BlockType::Water) {
    return; // solid, can't flow into an occupied cell
  }
  objects[occupant->second].setLevel(level);
  dirtyChunks.insert(chunkKeyAt(pos));
  broadcastToChunk(chunkKeyAt(pos),
                   proto::pack(proto::Type::UpdateWaterLevel,
                               proto::UpdateWaterLevel{pos, level}),
                   true);
}

void Server::removeBlock(int index) {
  // Copied, because objects[index] is overwritten by the swap below.
  const Vector3 pos = objects[index].getTransform().pos;
  occupiedCells.erase(blockKey(pos));
  dirtyChunks.insert(chunkKeyAt(pos));

  // Take `index` out of its chunk's list. This must come before the swap
  // fix-up below (see there).
  auto chunk = chunkBlocks.find(chunkKeyAt(pos));
  std::erase(chunk->second, index);
  if (chunk->second.empty()) {
    chunkBlocks.erase(
        chunk); // no empty lists, so "chunk exists" means "chunk has blocks"
  }

  // Swap-and-pop, so only the moved block's index needs fixing up.
  int last = (int)objects.size() - 1;
  if (index != last) {
    const Vector3 movedPos = objects[last].getTransform().pos;
    objects[index] = objects[last];
    occupiedCells[blockKey(movedPos)] = index;
    // The moved block was listed as `last` in its chunk; it is `index` now.
    // If it shares a chunk with the removed block, doing this before the
    // erase above would leave `index` listed twice.
    std::vector<int> &moved = chunkBlocks[chunkKeyAt(movedPos)];
    std::replace(moved.begin(), moved.end(), last, index);
  }
  objects.pop_back();

  // Water beside or above the gap may now flow into it.
  const int64_t neighborKeys[] = {
      blockKey(Vector3Add(pos, {BLOCK_SIZE, 0, 0})),
      blockKey(Vector3Add(pos, {-BLOCK_SIZE, 0, 0})),
      blockKey(Vector3Add(pos, {0, 0, BLOCK_SIZE})),
      blockKey(Vector3Add(pos, {0, 0, -BLOCK_SIZE})),
      blockKey(Vector3Add(pos, {0, BLOCK_SIZE, 0}))};

  for (const int64_t key : neighborKeys) {
    auto it = occupiedCells.find(key);
    if (it != occupiedCells.end() &&
        objects[it->second].getType() == BlockType::Water)
      fluidSim.markActive(key);
  }
}

int Server::findBlockHit(Vector3 from, Vector3 to) const {
  Vector3 lo = Vector3Min(from, to);
  Vector3 hi = Vector3Max(from, to);

  int best = -1;
  float bestDistSq = FLT_MAX;

  // Only the cells the segment's bounding box spans can hold a block it
  // touches.
  for (int y = (int)floorf(lo.y / BLOCK_SIZE);
       y <= (int)floorf(hi.y / BLOCK_SIZE); y++) {
    for (int z = (int)floorf(lo.z / BLOCK_SIZE);
         z <= (int)floorf(hi.z / BLOCK_SIZE); z++) {
      for (int x = (int)floorf(lo.x / BLOCK_SIZE);
           x <= (int)floorf(hi.x / BLOCK_SIZE); x++) {
        auto it = occupiedCells.find(cellKey(x, y, z));
        if (it == occupiedCells.end())
          continue;
        if (objects[it->second].getType() == BlockType::Water)
          continue; // bullets pass through water

        const ObjectTransform &t = objects[it->second].getTransform();
        Vector3 half = Vector3Scale(t.scale, 0.5f);
        BoundingBox box{Vector3Subtract(t.pos, half), Vector3Add(t.pos, half)};
        if (!SegmentIntersectsBox(from, to, box))
          continue;

        float distSq = Vector3DistanceSqr(from, t.pos);
        if (distSq < bestDistSq) {
          bestDistSq = distSq;
          best = it->second;
        }
      }
    }
  }
  return best;
}

void Server::tick(float dt) {
  saveCountdown -= dt;
  if (saveCountdown <= 0) {
    saveCountdown = saveCountdownTime;
    saveWorldAsync();
    checkOverlaps();
  }
  auto tickStart = std::chrono::steady_clock::now();
  for (Player &p : players) {
    p.shotBudget =
        std::min(env::SHOT_BURST, p.shotBudget + dt / env::SHOT_INTERVAL);
  }
  // ==== hit detection ==== //
  for (auto &b : bullets) {
    b.deathCountdown -= dt;

    Vector3 prevPos = b.pos;
    auto cell =
        occupiedCells.find(cellKey((int)floorf(prevPos.x / BLOCK_SIZE),
                                   (int)floorf(prevPos.y / BLOCK_SIZE),
                                   (int)floorf(prevPos.z / BLOCK_SIZE)));
    bool inWater = cell != occupiedCells.end() &&
                   objects[cell->second].getType() == BlockType::Water;
    b.pos =
        Vector3Add(b.pos, Vector3Scale(b.vel, dt * (inWater ? 0.8f : 1.0f)));

    int hit = findBlockHit(prevPos, b.pos);
    if (hit >= 0) {
      b.deathCountdown = 0.0f; // bullet is spent on the first block it hits

      Object &o = objects[hit];
      o.damage();
      const Vector3 pos = o.getTransform().pos;
      const int64_t chunk = chunkKeyAt(pos);
      dirtyChunks.insert(chunk); // damaged in place, still an edit
      if (o.getDurability() <= 0) {
        broadcastToChunk(
            chunk,
            proto::pack(proto::Type::RemoveObject, proto::RemoveObject{pos}),
            true);
        removeBlock(hit); // invalidates `o`
      } else {
        broadcastToChunk(
            chunk,
            proto::pack(proto::Type::DamageObject, proto::DamageObject{pos}),
            true);
      }
    }

    for (auto &p : players) {
      if (b.deathCountdown <= 0.0f) {
        break; // already spent on a block this tick
      }
      if (p.id == b.playerId) {
        continue; // don't hit the shooter
      }
      BoundingBox player;
      player.min = Vector3Subtract(
          p.pos, {PLAYER_SCALE.x * 0.5f, 0.0f, PLAYER_SCALE.z * 0.5f});
      player.max = Vector3Add(player.min, PLAYER_SCALE);

      if (SegmentIntersectsBox(prevPos, b.pos, player)) {
        b.deathCountdown = 0.0f;
        p.health--;
        // Before any reset below, so clients see health <= 0 and count the
        // kill.
        broadcast(proto::pack(proto::Type::PlayerHit,
                              proto::PlayerHit{p.health, p.id, b.playerId}),
                  true);

        if (p.health <= 0) {
          Vector3 spawnPos = randomSpawn();
          p.health = env::PLAYER_MAX_HEALTH;
          p.pos = spawnPos;
          sendTo(p.id,
                 proto::pack(proto::Type::Respawn, proto::Respawn{spawnPos}),
                 true);
          kills[b.playerId] += 1;
        }
        break;
      }
    }

    if (b.pos.y <= 0) {
      b.deathCountdown = 0.0f;
    }

    if (b.deathCountdown <= 0.0f) {
      broadcast(proto::pack(proto::Type::DeleteBullet,
                            proto::DeleteBullet{b.bulletId}),
                true);
    }
  }

  bullets.erase(
      std::remove_if(bullets.begin(), bullets.end(),
                     [](const Bullet &b) { return b.deathCountdown <= 0.0f; }),
      bullets.end());

  // ==== interest management ==== //
  for (const Player &p : players) {
    updateView(p);
  }

  static float debugPrintCountdown = 0.0f;
  debugPrintCountdown -= dt;
  if (debugPrintCountdown <= 0.0f) {
    debugPrintCountdown = 1.0f;
    double tickMs = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - tickStart)
                        .count();
    std::printf("tick: %.2f ms (objects=%zu bullets=%zu players=%zu)\n", tickMs,
                objects.size(), bullets.size(), players.size());
  }

  fluidSim.tick(
      dt, FluidWorld{objects, occupiedCells,
                     [&](Vector3 pos, int level) {
                       setWaterLevel(pos, (uint8_t)level);
                     },
                     [&](int index) {
                       const Vector3 pos = objects[index].getTransform().pos;
                       broadcastToChunk(chunkKeyAt(pos),
                                        proto::pack(proto::Type::RemoveObject,
                                                    proto::RemoveObject{pos}),
                                        true);
                       removeBlock(index);
                     }});
}

// ==== transport plumbing ==== //
void Server::pumpEnet() {
  ENetEvent event;
  // Doubles as the loop's pacing: blocks up to ~1 frame waiting for traffic.
  if (enet_host_service(host, &event, 16) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT: {
      const int id =
          handleConnect(std::make_unique<EnetConnection>(event.peer));
      event.peer->data = reinterpret_cast<void *>(static_cast<intptr_t>(id));
      break;
    }

    case ENET_EVENT_TYPE_DISCONNECT:
      handleDisconnect(
          static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data)));
      break;

    case ENET_EVENT_TYPE_RECEIVE: {
      const int id =
          static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data));
      std::string data(reinterpret_cast<char *>(event.packet->data),
                       event.packet->dataLength);
      enet_packet_destroy(event.packet);
      handleReceive(id, data);
      break;
    }

    case ENET_EVENT_TYPE_NONE:
      break;
    }
  }
}

void Server::pumpWebSockets() {
  if (!wsProxy) {
    return;
  }

  for (auto &event : wsProxy->drain()) {
    switch (event.kind) {
    case WsEvent::Kind::Connect:
      wsPlayerIds[event.socketId] = handleConnect(std::move(event.connection));
      break;

    case WsEvent::Kind::Message: {
      auto it = wsPlayerIds.find(event.socketId);
      if (it != wsPlayerIds.end()) {
        handleReceive(it->second, event.data);
      }
      break;
    }

    case WsEvent::Kind::Disconnect: {
      auto it = wsPlayerIds.find(event.socketId);
      if (it != wsPlayerIds.end()) {
        handleDisconnect(it->second);
        wsPlayerIds.erase(it);
      }
      break;
    }
    }
  }
}

// ==== main loop ==== //
void Server::poll() {
  static auto lastTick = std::chrono::steady_clock::now();

  pumpEnet();
  pumpWebSockets();

  auto now = std::chrono::steady_clock::now();
  float dt = std::chrono::duration<float>(now - lastTick).count();
  if (dt >= env::TICK_RATE) {
    tick(std::min(dt, env::MAX_TICK_DT)); // after a stall, don't sweep bullets
                                          // across huge boxes in one step
    lastTick = now;
  }
}
