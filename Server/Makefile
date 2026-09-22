CXX      := c++
CXXFLAGS := -std=c++20 -Isrc

test: test_chunk test_terrain
	@/tmp/chunk_test
	@/tmp/terrain_test

test_chunk: test_chunk.cpp src/Server/chunk.hpp
	$(CXX) $(CXXFLAGS) test_chunk.cpp -o /tmp/chunk_test

test_terrain: terrain_test.cpp src/Server/terrain.cpp src/Server/terrain.hpp
	$(CXX) $(CXXFLAGS) terrain_test.cpp src/Server/terrain.cpp -o /tmp/terrain_test

.PHONY: test test_chunk test_terrain
