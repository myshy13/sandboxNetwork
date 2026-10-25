#pragma once

#include <memory>
#include <optional>
#include <string>

// status 0 means the request never got an answer (no network, TLS failure).
struct HttpResponse {
  int status{0};
  std::string body;
};

// One HTTPS client; CMake compiles exactly one backend (see Transport).
class HttpClient {
 public:
  virtual ~HttpClient() = default;

  // Non-blocking: starts a JSON POST and returns; ignored while busy().
  virtual void post(const std::string& url, const std::string& jsonBody,
                    const std::string& authorization = "") = 0;

  // True from post() until poll() has handed the response back.
  virtual bool busy() const = 0;

  // The finished response, once; nullopt while in flight or idle. Call every frame.
  virtual std::optional<HttpResponse> poll() = 0;
};

// Defined by whichever backend CMake compiled in.
std::unique_ptr<HttpClient> makeHttpClient();
