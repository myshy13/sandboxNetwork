#include <httplib.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "Net/httpClient.hpp"

enum class State { Idle, Running, Done };

constexpr time_t CONNECT_TIMEOUT_SECONDS = 10;
constexpr time_t READ_TIMEOUT_SECONDS = 10;
constexpr const char* CA_BUNDLE_PATH = "assets/cacert.pem";

class HttpClientNative : public HttpClient {
 private:
  std::thread worker;
  mutable std::mutex stateMutex;
  State state = State::Idle;
  std::optional<HttpResponse> result;

  // Runs on the worker thread: no raylib calls in here.
  void run(const std::string& url, const std::string& jsonBody,
           const std::string& authorization) {
    HttpResponse response;  // status 0 until the server answers

    try {
      // Split "https://host:port/path" into the origin and the path.
      const size_t schemeEnd = url.find("://");
      const size_t pathStart = schemeEnd == std::string::npos
                                   ? schemeEnd
                                   : url.find('/', schemeEnd + 3);
      if (schemeEnd != std::string::npos) {
        const std::string origin = url.substr(0, pathStart);
        const std::string path =
            pathStart == std::string::npos ? "/" : url.substr(pathStart);

        httplib::Client client(origin);
        httplib::Headers headers;
        if (!authorization.empty())
          headers.emplace("authorization", authorization);
        client.set_connection_timeout(CONNECT_TIMEOUT_SECONDS);
        client.set_read_timeout(READ_TIMEOUT_SECONDS);
        client.set_ca_cert_path(CA_BUNDLE_PATH);  // verification stays on

        if (auto res =
                client.Post(path, headers, jsonBody, "application/json")) {
          response.status = res->status;
          response.body = res->body;
        }
      }
    } catch (...) {
      // an exception leaving a thread would terminate the game
    }

    // Result and state are published together under the lock.
    std::lock_guard<std::mutex> lock(stateMutex);
    result = std::move(response);
    state = State::Done;
  }

 public:
  void post(const std::string& url, const std::string& jsonBody,
            const std::string& authorization) override {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (state != State::Idle) return;
    if (worker.joinable()) worker.join();
    state = State::Running;
    worker =
        std::thread(&HttpClientNative::run, this, url, jsonBody, authorization);
  }

  bool busy() const override {
    std::lock_guard<std::mutex> lock(stateMutex);
    return state != State::Idle;
  }

  std::optional<HttpResponse> poll() override {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (state != State::Done) return std::nullopt;
    std::optional<HttpResponse> out = std::move(result);
    result.reset();
    state = State::Idle;
    return out;
  }

  // Only joinable once post() has run; joining a never-started thread throws,
  // and a destructor can't.
  ~HttpClientNative() {
    if (worker.joinable()) worker.join();
  }
};

std::unique_ptr<HttpClient> makeHttpClient() {
  return std::make_unique<HttpClientNative>();
}
