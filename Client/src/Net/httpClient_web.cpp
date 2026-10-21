
#include <emscripten/fetch.h>

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "Net/httpClient.hpp"

// A hung server must surface as status 0 ("Can't reach server"), not leave Auth
// in Working forever.
constexpr unsigned long REQUEST_TIMEOUT_MS = 10000;

class HttpClientWeb final : public HttpClient {
 private:
  enum class State { Idle, Running, Done };

  State state{State::Idle};
  emscripten_fetch_t* request{nullptr};
  std::optional<HttpResponse> result;
  std::string url;
  std::string body;

  static void onSuccess(emscripten_fetch_t* fetch) {
    auto* client = static_cast<HttpClientWeb*>(fetch->userData);
    if (client == nullptr) return;
    client->finish(fetch);
  }

  static void onError(emscripten_fetch_t* fetch) { onSuccess(fetch); }

  void finish(emscripten_fetch_t* fetch) {
    HttpResponse response;
    response.status = fetch->status;
    if (fetch->data != nullptr && fetch->numBytes > 0) {
      response.body.assign(fetch->data,
                           static_cast<std::size_t>(fetch->numBytes));
    }

    request = nullptr;
    result = std::move(response);
    state = State::Done;
    emscripten_fetch_close(fetch);
  }

 public:
  ~HttpClientWeb() override {
    if (request != nullptr) {
      request->userData = nullptr;
      emscripten_fetch_close(request);
      request = nullptr;
    }
  }

  void post(const std::string& requestUrl,
            const std::string& jsonBody) override {
    if (state != State::Idle) return;

    url = requestUrl;
    body = jsonBody;
    state = State::Running;

    emscripten_fetch_attr_t attributes;
    emscripten_fetch_attr_init(&attributes);
    std::memcpy(attributes.requestMethod, "POST", sizeof("POST"));
    attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attributes.requestData = body.c_str();
    attributes.requestDataSize = body.size();
    attributes.userData = this;
    attributes.timeoutMSecs = REQUEST_TIMEOUT_MS;
    attributes.onsuccess = onSuccess;
    attributes.onerror = onError;
    const char* headers[] = {"Content-Type", "application/json", nullptr};
    attributes.requestHeaders = headers;

    request = emscripten_fetch(&attributes, url.c_str());
    if (request == nullptr) {
      result = HttpResponse{};
      state = State::Done;
    }
  }

  bool busy() const override { return state != State::Idle; }

  std::optional<HttpResponse> poll() override {
    if (state != State::Done) return std::nullopt;

    std::optional<HttpResponse> response = std::move(result);
    result.reset();
    state = State::Idle;
    url.clear();
    body.clear();
    return response;
  }
};

std::unique_ptr<HttpClient> makeHttpClient() {
  return std::make_unique<HttpClientWeb>();
}
