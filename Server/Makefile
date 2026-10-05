CXX      := c++
DEPS     := ../Client/build/_deps
CXXFLAGS := -std=c++20 -Isrc -I../Shared -I$(DEPS)/cereal-src/include -I$(DEPS)/raylib-src/src

test: test_chunk test_terrain test_fluid
	@/tmp/chunk_test
	@/tmp/test_terrain
	@/tmp/fluid_test

test_chunk: test_chunk.cpp src/Terrain/chunk.hpp
	$(CXX) $(CXXFLAGS) test_chunk.cpp -o /tmp/chunk_test

test_terrain: test_terrain.cpp src/Terrain/terrain.cpp src/Terrain/terrain.hpp
	$(CXX) $(CXXFLAGS) test_terrain.cpp src/Terrain/terrain.cpp -o /tmp/test_terrain

test_fluid: test_fluid.cpp src/Fluid/fluidSim.cpp src/Fluid/fluidSim.hpp src/Server/blockHelpers.hpp
	$(CXX) $(CXXFLAGS) test_fluid.cpp src/Fluid/fluidSim.cpp -o /tmp/fluid_test

.PHONY: test test_chunk test_terrain test_fluid
