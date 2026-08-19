#include "Server/server.hpp"

int main() {
  Server server;

  while (true) {
    server.poll();
  }

  return 0;
}
