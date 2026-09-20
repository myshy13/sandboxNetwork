CXX      := c++
CXXFLAGS := -std=c++20 -Isrc

test: test_chunk.cpp src/Server/chunk.hpp
	$(CXX) $(CXXFLAGS) test_chunk.cpp -o /tmp/chunk_test
	/tmp/chunk_test

.PHONY: test
