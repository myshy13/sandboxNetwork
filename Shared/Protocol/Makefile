CXX      := c++
CXXFLAGS := -std=c++17 -I.. -I../../Client/build/_deps/cereal-src/include -I../../Client/build/_deps/raylib-src/src

test: test_protocol.cpp protocol.cpp protocol.hpp
	$(CXX) $(CXXFLAGS) test_protocol.cpp protocol.cpp -o /tmp/protocol_test
	/tmp/protocol_test

.PHONY: test
