CXX      := c++
DEPS     := ../Client/build/_deps
CXXFLAGS := -std=c++20 -Isrc -I../Shared -I$(DEPS)/cereal-src/include -I$(DEPS)/raylib-src/src

test: test_chunk test_terrain test_fluid
	@/tmp/chunk_test
	@/tmp/terrain_test
	@/tmp/fluid_test

test_chunk: test_chunk.cpp src/Terrain/chunk.hpp
	$(CXX) $(CXXFLAGS) test_chunk.cpp -o /tmp/chunk_test

test_terrain: terrain_test.cpp src/Terrain/terrain.cpp src/Terrain/terrain.hpp
	$(CXX) $(CXXFLAGS) terrain_test.cpp src/Terrain/terrain.cpp -o /tmp/terrain_test

test_fluid: test_fluid.cpp src/Fluid/fluidSim.cpp src/Fluid/fluidSim.hpp src/Server/blockHelpers.hpp
	$(CXX) $(CXXFLAGS) test_fluid.cpp src/Fluid/fluidSim.cpp -o /tmp/fluid_test

.PHONY: test test_chunk test_terrain test_fluid
