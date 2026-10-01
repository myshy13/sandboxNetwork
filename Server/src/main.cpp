#include "Server/server.hpp"
#include "Terrain/chunk.hpp"
#include "env.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

std::atomic<bool> keep_running(true);

void sigIntHandler(int signal_num) {
  if (signal_num == SIGINT) {
    keep_running = false;
  }
}

int main(int argc, char **argv) {
  // The same seed (and WORLD_SIZE) gives the same terrain; defaults to the
  // time.
  unsigned seed = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
  int wsPort = 0;
  std::string savePath = "save";
  int saveTime = 30;
  int maxPlayers = env::DEFAULT_MAX_PLAYERS;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--ws-port") == 0 && i + 1 < argc) {
      wsPort = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--save-path") == 0 && i + 1 < argc) {
      savePath = argv[++i];
    } else if (std::strcmp(argv[i], "--save-time") == 0 && i + 1 < argc) {
      saveTime = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
    } else if (std::strcmp(argv[i], "--max-players") == 0 && i + 1 < argc) {
      maxPlayers = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::printf("usage: %s [--ws-port <port>] [--save-path <path>] "
                  "[--save-time <seconds>] [--seed <n>]\n\n"
                  "  --ws-port <port>  also accept browser clients over "
                  "WebSocket on <port>.\n"
                  "                    Needed for the Emscripten build, which "
                  "cannot use raw UDP.\n"
                  "  --save-path <path>  specify the path to save game data.\n"
                  "  --save-time <seconds>  specify the time interval between "
                  "world saves.\n"
                  "  --seed <n>  seed the terrain generator (default: the "
                  "current time).\n"
                  "  --max-players <players> specifies a limit to the player "
                  "count.\n",
                  argv[0]);
      return 0;
    } else {
      std::fprintf(stderr, "unknown argument: %s (try --help)\n", argv[i]);
      return EXIT_FAILURE;
    }
  }

  // Refuse an old save rather than lose it: old chunk files can't be read, and
  // unedited chunks would generate differently next to the saved ones.
  auto meta = readMetaFile(metaFilePath(savePath));
  if (meta.has_value() && (meta->saveFormatVersion != env::saveFormatVersion ||
                           meta->terrainVersion != env::terrainVersion)) {
    std::fprintf(
        stderr,
        "%s was saved by an older server (save format %u, terrain %u; "
        "this server writes %u, %u).\nMove it aside or use --save-path "
        "to start a new world.\n",
        savePath.c_str(), meta->saveFormatVersion, meta->terrainVersion,
        env::saveFormatVersion, env::terrainVersion);
    return EXIT_FAILURE;
  }

  // After arg parsing so --help and bad args still reach the terminal.
  std::printf("logging to server.log\n");
  std::freopen("server.log", "a", stdout);
  std::freopen("server.log", "a", stderr);
  std::setvbuf(stdout, nullptr, _IOLBF,
               0); // a file is fully buffered by default

  // A saved seed overrides the CLI/current-time one: unedited chunks have to
  // regenerate with the same terrain as the edited chunks already on disk.
  int nextObjectId = 1;
  if (meta.has_value()) {
    seed = meta->seed;
    nextObjectId = meta->nextObjectId;
  }

  std::printf("seed: %u\n", seed);
  srand(seed); // before the Server exists: its constructor generates the world
  Server server(wsPort, savePath, saveTime, seed, nextObjectId, maxPlayers);
  std::signal(SIGINT, sigIntHandler);

  while (keep_running) {
    server.poll();
  }

  std::printf("Saving world... \n");
  server.saveWorld();
  std::printf("Done... shutting down now\n");

  return 0;
}
