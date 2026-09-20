
#include "Server/server.hpp"
#include "Models/Object.hpp"
#include "Protocol/protocol.hpp"
#include "Server/chunk.hpp"
#include "enet/enet.h"
#include "env.hpp"
#include "raylib.h"
#include <algorithm>
#include <cereal/types/vector.hpp>
#include <cfloat>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <raymath.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
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

Server::Server(int wsPort, std::string savePath, int saveTime)
    : savePath(std::move(savePath)), saveTime(saveTime) {
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

  loadWorld();
  checkChunkIndex(); // TEMP
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
constexpr float BLOCK_SIZE =
    5.0f; // same as the client's blockSize (Client/src/World/world.cpp)

static constexpr float CHUNK_SIZE =
    16 * BLOCK_SIZE; // must match World::STREAM_CHUNK_SIZE
                     // (Client/src/World/world.hpp)

// Packs a grid cell's (x, y, z) into one hashable key, offset so negative cells
// don't collide.
static int64_t cellKey(int x, int y, int z) {
  constexpr int64_t OFFSET = 1 << 20;
  return ((x + OFFSET) << 42) | ((y + OFFSET) << 21) | (z + OFFSET);
}

static int64_t blockKey(Vector3 pos) {
  return cellKey((int)floorf(pos.x / BLOCK_SIZE),
                 (int)floorf(pos.y / BLOCK_SIZE),
                 (int)floorf(pos.z / BLOCK_SIZE));
}

// The key of the chunk a world position falls in.
static int64_t chunkKeyAt(Vector3 pos) {
  // Divide and floor while still a float; a plain (int) cast rounds toward
  // zero, which is wrong below 0.
  return chunkKey((int)floorf(pos.x / CHUNK_SIZE),
                  (int)floorf(pos.z / CHUNK_SIZE));
}

struct WorldSave {
  std::vector<Object> objects{};
  int nextObjectId{1};
  template <class A> void serialize(A &ar) { ar(objects, nextObjectId); }
};

// ==== World saving ==== //
// Writes to a temp file and renames it over the save, so a crash mid-write
// can't corrupt the old save. Runs on any thread: it only touches `save`.
static bool writeSave(const std::string &path, const WorldSave &save) {
  try {
    const std::string tmp = path + ".tmp";
    {
      std::ofstream os(tmp, std::ios::binary);
      cereal::BinaryOutputArchive ar(os);
      ar(save);
      os.flush();
      if (!os)
        return false;
    }
    std::filesystem::rename(tmp, path);
    return true;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "save failed (%s)\n", e.what());
    return false;
  }
}

// Blocks until any background save has finished, then writes synchronously.
// Used at shutdown, where the write has to be done before we exit.
void Server::saveWorld() {
  if (saving.valid() && !saving.get())
    worldChanged = true; // the background write failed, so redo it
  if (!worldChanged)
    return; // the file on disk is already current

  WorldSave save;
  save.objects = objects;
  save.nextObjectId = nextObjectId;
  if (writeSave(savePath, save))
    worldChanged = false;
}

// Snapshots the world here (a memory copy), then writes it on another thread
// so the tick never waits on the disk.
void Server::saveWorldAsync() {
  if (saving.valid()) {
    if (saving.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      return; // still writing the last one, try again next period
    if (!saving.get())
      worldChanged = true; // it failed, so retry with fresh data
  }
  if (!worldChanged)
    return;
  worldChanged = false;
  checkChunkIndex(); // TEMP: re-verify after block edits

  WorldSave snapshot;
  snapshot.objects = objects;
  snapshot.nextObjectId = nextObjectId;
  saving = std::async(std::launch::async,
                      [path = savePath, snapshot = std::move(snapshot)] {
                        return writeSave(path, snapshot);
                      });
}

void Server::loadWorld() {
  std::ifstream is(savePath, std::ios::binary);
  if (!is) {
    std::printf("no save at %s, starting fresh\n", savePath.c_str());
    generateWorld();
    return;
  }

  try {
    WorldSave save;
    cereal::BinaryInputArchive ar(is);
    ar(save); // reads objects + nextObjectId back out

    objects = std::move(save.objects);
    nextObjectId = save.nextObjectId;
    for (int i = 0; i < (int)objects.size(); i++) {
      indexBlock(i); // not addBlock: the block is already in `objects`, and
                     // this isn't a change to save
    }
    std::printf("loaded %zu objects from %s\n", objects.size(),
                savePath.c_str());
  } catch (const cereal::Exception &e) {
    std::fprintf(stderr, "save file corrupt (%s), starting fresh\n", e.what());
    objects.clear();
    occupiedCells.clear();
    chunkBlocks.clear();
    nextObjectId = 1;
    generateWorld();
  }
}

void Server::generateWorld() {
  std::cout << "Generating World\n";
  const auto genStart = std::chrono::steady_clock::now();
  // World spans [-WORLD_SIZE, WORLD_SIZE) on both axes; SPAN is the grid's
  // actual width/height, and toIndex offsets x/z so they're never negative.
  constexpr int WORLD_SIZE = 200;
  constexpr int SPAN = 2 * WORLD_SIZE;
  auto toIndex = [](int x, int z) {
    return (z + WORLD_SIZE) * SPAN + (x + WORLD_SIZE);
  };

  std::vector<int> heightMap(SPAN * SPAN);
  Vector3 blockSize = {5, 5, 5};

  std::cout << "Generating Terrain\n";
  for (int z = -WORLD_SIZE; z < WORLD_SIZE; z++) {
    for (int x = -WORLD_SIZE; x < WORLD_SIZE; x++) {
      heightMap[toIndex(x, z)] = rand() % 20;
    }
  }

  std::cout << "Smoothing terrain\n";
  for (int i = 0; i < 7; i++) {
    std::vector<int> smoothed(heightMap.size());
    for (int z = -WORLD_SIZE; z < WORLD_SIZE; z++) {
      for (int x = -WORLD_SIZE; x < WORLD_SIZE; x++) {
        int sum = 0, count = 0;
        for (int dz = -1; dz <= 1; dz++) {
          for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, nz = z + dz;
            if (nx < -WORLD_SIZE || nx >= WORLD_SIZE || nz < -WORLD_SIZE ||
                nz >= WORLD_SIZE)
              continue;
            sum += heightMap[toIndex(nx, nz)];
            count++;
          }
        }
        smoothed[toIndex(x, z)] = sum / count;
      }
    }
    heightMap = std::move(smoothed);
  }

  std::cout << "Lowest terrain\n";
  int lowest = INT_MAX;
  for (int z = -WORLD_SIZE; z < WORLD_SIZE; z++) {
    for (int x = -WORLD_SIZE; x < WORLD_SIZE; x++) {
      lowest = std::min(lowest, heightMap[toIndex(x, z)]);
    }
  }

  std::cout << "Lowering terrain\n";
  for (int z = -WORLD_SIZE; z < WORLD_SIZE; z++) {
    for (int x = -WORLD_SIZE; x < WORLD_SIZE; x++) {
      heightMap[toIndex(x, z)] -= lowest;
    }
  }

  std::cout << "Building terrain\n";
  for (int z = -WORLD_SIZE; z < WORLD_SIZE; z++) {
    for (int x = -WORLD_SIZE; x < WORLD_SIZE; x++) {
      int height = heightMap[toIndex(x, z)];
      float blockX = x * blockSize.x + (blockSize.x / 2);
      float blockY = height * blockSize.y + (blockSize.y / 2);
      float blockZ = z * blockSize.z + (blockSize.z / 2);

      Object o(nextObjectId,
               ObjectTransform{{blockX, blockY, blockZ}, blockSize}, GREEN);
      int damage = rand() % 2;
      for (int i = 0; i < damage; i++) {
        o.damage();
      }
      addBlock(o);
      nextObjectId++;

      for (int i = height; i > 0; i--) {
        blockY -= blockSize.y;
        Object o(nextObjectId,
                 ObjectTransform{{blockX, blockY, blockZ}, blockSize}, BROWN);
        int damage = rand() % 3;
        for (int i = 0; i < damage; i++) {
          o.damage();
        }
        addBlock(o);
        nextObjectId++;
      }
    }
  }
  const double genSeconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - genStart)
          .count();
  std::cout << "Done building world: " << objects.size() << " blocks in "
            << genSeconds << " s\n";
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
  Vector3 spawnPos;
  spawnPos.x = rand() % 200 - 100;
  spawnPos.z = rand() % 200 - 100;
  spawnPos.y = 10;
  newPlayer.pos = spawnPos;
  players.push_back(newPlayer);

  // handshake stuff
  sendTo(id, proto::pack(proto::Type::GivenId, proto::GivenId{id}), true);
  sendTo(id, proto::pack(proto::Type::Respawn, proto::Respawn{spawnPos}), true);

  // Stream the world in chunks rather than one huge message, so the client
  // indexes it incrementally instead of stalling on a single collision-grid
  // rebuild for the whole world.
  const auto syncStart = std::chrono::steady_clock::now();
  size_t syncBytes = 0;
  for (size_t i = 0; i < objects.size(); i += env::WORLD_SYNC_CHUNK_SIZE) {
    size_t end = std::min(i + env::WORLD_SYNC_CHUNK_SIZE, objects.size());
    proto::initBlocks chunk{{objects.begin() + i, objects.begin() + end}};
    const std::string packed = proto::pack(proto::Type::initBlocks, chunk);
    syncBytes += packed.size();
    sendTo(id, packed, true);
  }
  // Baseline for the streaming plan (plan.md): this is packing + queueing, not
  // delivery time.
  const double syncMs = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - syncStart)
                            .count();
  std::printf(
      "world sync to client %d: %zu blocks, %.1f MB, %.0f ms to queue\n", id,
      objects.size(), syncBytes / 1e6, syncMs);
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

  switch (proto::peekType(data)) {
  // ==== pos update handler ==== //
  case proto::Type::PlayerUpdate: {
    auto msg = proto::unpack<proto::PlayerUpdate>(data);
    msg.id = playerId; // trust the connection, not the payload

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
    std::optional<Bullet> bullet = createBullet(playerId, msg.origin, msg.dir);
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
    broadcast(data, true);
    break;
  }

  case proto::Type::SetName: {
    auto msg = proto::unpack<proto::SetName>(data);
    if (auto *player = findPlayer(playerId)) {
      for (Player &p : players) {
        if (msg.name == p.displayName) {
          break;
        }
      }
      player->displayName = msg.name;
      broadcast(data, true);
    }
    break;
  }

  case proto::Type::PlaceObject: {
    auto msg = proto::unpack<proto::PlaceObject>(data);
    if (occupiedCells.contains(blockKey(msg.object.getTransform().pos))) {
      break; // one block per cell
    }
    msg.object.setId(nextObjectId++); // server owns ids, clients send -1
    addBlock(msg.object);
    broadcast(proto::pack(proto::Type::NewObject, proto::NewObject{msg.object}),
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
}

// ==== fixed-rate tick (bullet lifetime etc.) ==== //

// same as the client's default Player scale (Client/src/Player/player.cpp)
constexpr Vector3 PLAYER_SCALE = {1.5f, 10.0f, 1.5f};

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
  // find, not []: [] would insert an empty list. An empty chunk is still sent,
  // so the client knows it has loaded.
  auto it = chunkBlocks.find(chunkKey(cx, cz));
  if (it != chunkBlocks.end()) {
    msg.blocks.reserve(it->second.size());
    for (int i : it->second) {
      msg.blocks.push_back(objects[i]); // a copy: the snapshot at this moment
    }
  }
  std::printf("TEMP load chunk (%d, %d), %zu blocks, to player %d\n", cx, cz,
              msg.blocks.size(), playerId);
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
    std::printf("TEMP unload chunk (%d, %d) from player %d\n", cx, cz, p.id);
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
    sendChunk(p.id, cx, cz);
    view.loaded.insert(chunkKey(cx, cz));
  }
}

void Server::addBlock(const Object &block) {
  objects.push_back(block);
  indexBlock((int)objects.size() - 1);
  worldChanged = true;
}

void Server::removeBlock(int index) {
  // Copied, because objects[index] is overwritten by the swap below.
  const Vector3 pos = objects[index].getTransform().pos;
  occupiedCells.erase(blockKey(pos));

  // Take `index` out of its chunk's list. This must come before the swap fix-up
  // below (see there).
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
    // The moved block was listed as `last` in its chunk; it is `index` now. If
    // it shares a chunk with the removed block, doing this before the erase
    // above would leave `index` listed twice.
    std::vector<int> &moved = chunkBlocks[chunkKeyAt(movedPos)];
    std::replace(moved.begin(), moved.end(), last, index);
  }
  objects.pop_back();
  worldChanged = true;
}

void Server::checkChunkIndex() const {
  size_t listed = 0;
  size_t wrong = 0;
  for (const auto &[key, list] : chunkBlocks) {
    listed += list.size();
    for (int i : list) {
      if (chunkKeyAt(objects[i].getTransform().pos) != key) {
        wrong++;
      }
    }
  }
  const double average =
      chunkBlocks.empty() ? 0.0 : (double)listed / chunkBlocks.size();
  std::printf("chunk index: %zu chunks, %zu blocks listed (objects=%zu), %zu "
              "in the wrong chunk, %.0f blocks/chunk\n",
              chunkBlocks.size(), listed, objects.size(), wrong, average);
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
  }
  auto tickStart = std::chrono::steady_clock::now();
  // ==== hit detection ==== //
  for (auto &b : bullets) {
    b.deathCountdown -= dt;

    Vector3 prevPos = b.pos;
    b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt));

    int hit = findBlockHit(prevPos, b.pos);
    if (hit >= 0) {
      b.deathCountdown = 0.0f; // bullet is spent on the first block it hits

      Object &o = objects[hit];
      o.damage();
      worldChanged = true;
      Vector3 pos = o.getTransform().pos;
      if (o.getDurability() <= 0) {
        broadcast(
            proto::pack(proto::Type::RemoveObject, proto::RemoveObject{pos}),
            true);
        removeBlock(hit); // invalidates `o`
      } else {
        broadcast(
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

        if (p.health <= 0) {
          Vector3 spawnPos;
          spawnPos.x = rand() % 200 - 100;
          spawnPos.z = rand() % 200 - 100;
          spawnPos.y = 10;
          p.health = env::PLAYER_MAX_HEALTH;
          p.pos = spawnPos;
          sendTo(p.id,
                 proto::pack(proto::Type::Respawn, proto::Respawn{spawnPos}),
                 true);
          kills[b.playerId] += 1;
        }

        broadcast(proto::pack(proto::Type::PlayerHit,
                              proto::PlayerHit{p.health, p.id, b.playerId}),
                  true);
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

  // Hit detection is a brute-force scan of every object per bullet per tick,
  // so this is where a big world (see generateWorld) is expected to hurt.
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
    tick(dt);
    lastTick = now;
  }
}
