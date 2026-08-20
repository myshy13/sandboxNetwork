#include "Server/server.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv) {
  int wsPort = 0;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--ws-port") == 0 && i + 1 < argc) {
      wsPort = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::printf("usage: %s [--ws-port <port>]\n\n"
                  "  --ws-port <port>  also accept browser clients over "
                  "WebSocket on <port>.\n"
                  "                    Needed for the Emscripten build, which "
                  "cannot use raw UDP.\n",
                  argv[0]);
      return 0;
    } else {
      std::fprintf(stderr, "unknown argument: %s (try --help)\n", argv[i]);
      return EXIT_FAILURE;
    }
  }

  Server server(wsPort);

  while (true) {
    server.poll();
  }

  return 0;
}
