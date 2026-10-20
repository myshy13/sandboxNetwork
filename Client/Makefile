CXX      := c++
CXXFLAGS := -std=c++20 -Isrc -Ibuild/_deps/raylib-src/src

test: test_tokenStore.cpp src/Auth/tokenStore_file.cpp src/Auth/tokenStore.hpp
	$(CXX) $(CXXFLAGS) test_tokenStore.cpp src/Auth/tokenStore_file.cpp -o /tmp/tokenStore_test
	@/tmp/tokenStore_test

.PHONY: test
