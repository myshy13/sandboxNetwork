#include "Server/server.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv) {
  srand(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());

  int wsPort = 0;
  std::string savePath = "save.bin";
  int saveTime = 30;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--ws-port") == 0 && i + 1 < argc) {
      wsPort = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--save-path") == 0 && i + 1 < argc) {
      savePath = argv[++i];
    } else if (std::strcmp(argv[i], "--save-time") == 0 && i + 1 < argc) {
      saveTime = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::printf("usage: %s [--ws-port <port>] [--save-path <path>] [--save-time <seconds>]\n\n"
                  "  --ws-port <port>  also accept browser clients over "
                  "WebSocket on <port>.\n"
                  "                    Needed for the Emscripten build, which "
                  "cannot use raw UDP.\n"
                  "  --save-path <path>  specify the path to save game data.\n"
                  "  --save-time <seconds>  specify the time interval between world saves.\n",
                  argv[0]);
      return 0;
    } else {
      std::fprintf(stderr, "unknown argument: %s (try --help)\n", argv[i]);
      return EXIT_FAILURE;
    }
  }

  Server server(wsPort, savePath, saveTime);

  while (true) {
    server.poll();
  }

  return 0;
}
