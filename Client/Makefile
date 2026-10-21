CXX      := c++
CXXFLAGS := -std=c++20 -Isrc -I../Shared -Ibuild/_deps/raylib-src/src -Ibuild/_deps/cereal-src/include

test: test_tokenStore test_auth

test_tokenStore: test_tokenStore.cpp src/Auth/tokenStore_file.cpp src/Auth/tokenStore.hpp
	$(CXX) $(CXXFLAGS) test_tokenStore.cpp src/Auth/tokenStore_file.cpp -o /tmp/tokenStore_test
	@/tmp/tokenStore_test

test_auth: test_auth.cpp src/Auth/auth.cpp src/Auth/auth.hpp
	$(CXX) $(CXXFLAGS) test_auth.cpp src/Auth/auth.cpp -o /tmp/auth_test
	@/tmp/auth_test

.PHONY: test test_tokenStore test_auth
