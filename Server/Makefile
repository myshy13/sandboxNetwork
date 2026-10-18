CXX      := c++
DEPS     := ../Client/build/_deps
# Fetched by Server's own CMake configure; no trailing comment, Make would keep the space in the value.
MC       := build/_deps/monocypher-src/src
CXXFLAGS := -std=c++20 -Isrc -I../Shared -I$(DEPS)/cereal-src/include -I$(DEPS)/raylib-src/src -I$(MC) -I$(MC)/optional

test: test_chunk test_terrain test_fluid test_ticket
	@/tmp/chunk_test
	@/tmp/test_terrain
	@/tmp/fluid_test
	@/tmp/ticket_test

test_chunk: test_chunk.cpp src/Terrain/chunk.hpp
	$(CXX) $(CXXFLAGS) test_chunk.cpp -o /tmp/chunk_test

test_terrain: test_terrain.cpp src/Terrain/terrain.cpp src/Terrain/terrain.hpp
	$(CXX) $(CXXFLAGS) test_terrain.cpp src/Terrain/terrain.cpp -o /tmp/test_terrain

test_fluid: test_fluid.cpp src/Fluid/fluidSim.cpp src/Fluid/fluidSim.hpp src/Server/blockHelpers.hpp
	$(CXX) $(CXXFLAGS) test_fluid.cpp src/Fluid/fluidSim.cpp -o /tmp/fluid_test

test_ticket: test_ticket.cpp src/Auth/tickets.cpp src/Auth/tickets.hpp src/Auth/base64.hpp
	$(CC) -c $(MC)/monocypher.c -o /tmp/monocypher.o
	$(CC) -c $(MC)/optional/monocypher-ed25519.c -I$(MC) -o /tmp/monocypher-ed25519.o
	$(CXX) $(CXXFLAGS) test_ticket.cpp src/Auth/tickets.cpp /tmp/monocypher.o /tmp/monocypher-ed25519.o -o /tmp/ticket_test

.PHONY: test test_chunk test_terrain test_fluid test_ticket
