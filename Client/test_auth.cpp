#include <cassert>
#include <cstdio>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <cereal/archives/json.hpp>

#include "Auth/auth.hpp"
#include "env.hpp"

// auth.cpp logs through raylib's TraceLog; the test supplies a silent one so raylib isn't linked.
extern "C" void TraceLog(int, const char*, ...) {}

// ==== fakes ==== //

struct Post {
  std::string url;
  std::string body;
};

// Single-flight like the real client: busy from post() until poll() hands the answer back.
class FakeHttp : public HttpClient {
 public:
  std::vector<Post> posts;
  bool inFlight{false};
  std::optional<HttpResponse> answer;

  void post(const std::string& url, const std::string& body) override {
    if (inFlight) return;
    posts.push_back({url, body});
    inFlight = true;
  }
  bool busy() const override { return inFlight; }
  std::optional<HttpResponse> poll() override {
    if (!answer) return std::nullopt;
    auto out = answer;
    answer.reset();
    inFlight = false;
    return out;
  }
  // The server's reply to the request in flight; the next poll() returns it.
  void reply(int status, const std::string& body = "") { answer = HttpResponse{status, body}; }
};

class FakeStore : public TokenStore {
 public:
  std::string token;
  std::function<void()> onSave;
  std::string load() override { return token; }
  bool save(const std::string& t) override {
    if (onSave) onSave();
    token = t;
    return true;
  }
  void clear() override { token.clear(); }
};

// ==== helpers ==== //

const std::string TOKEN_A(43, 'a');
const std::string TOKEN_B(43, 'b');
const std::string TOKEN_C(43, 'c');

std::string loginBody(const std::string& ticket, const std::string& token) {
  return "{\"ticket\":\"" + ticket + "\",\"refreshToken\":\"" + token + "\"}";
}

// Reads the flat JSON the client sent; throws if a key is missing or nested.
struct Creds {
  std::string name, password;
};
Creds parseCreds(const std::string& body) {
  Creds c;
  std::istringstream iss(body);
  cereal::JSONInputArchive ar(iss);
  ar(cereal::make_nvp("name", c.name), cereal::make_nvp("password", c.password));
  return c;
}
std::string parseRefreshToken(const std::string& body) {
  std::string t;
  std::istringstream iss(body);
  cereal::JSONInputArchive ar(iss);
  ar(cereal::make_nvp("refreshToken", t));
  return t;
}

struct Fixture {
  FakeHttp http;
  FakeStore store;
  Auth auth{store, http};

  // Answers the request in flight and lets Auth poll it at time `now`.
  void answer(int64_t now, int status, const std::string& body = "") {
    http.reply(status, body);
    auth.update(now);
  }
  // Logs in at time `now` and leaves the fixture LoggedIn with `token`.
  void loggedIn(int64_t now, const std::string& token = TOKEN_A) {
    assert(auth.login("alice", "hunter2hunter2"));
    answer(now, 200, loginBody("T1", token));
    assert(auth.getState() == State::LoggedIn);
  }
};

int main() {
  const std::string URL = env::AUTH_URL;

  // ==== a fresh Auth is logged out and sends nothing ==== //
  {
    Fixture f;
    f.auth.update(1000000);
    assert(f.auth.getState() == State::LoggedOut);
    assert(f.auth.getTicket().empty() && f.auth.getTicketSerial() == 0);
    assert(f.http.posts.empty());
  }

  // ==== login: posts flat name/password JSON to /login and shows Working ==== //
  {
    Fixture f;
    assert(f.auth.login("alice", "hunter2hunter2"));
    assert(f.http.posts.size() == 1);
    assert(f.http.posts[0].url == URL + "/login");
    Creds c = parseCreds(f.http.posts[0].body);
    assert(c.name == "alice" && c.password == "hunter2hunter2");
    assert(f.auth.getState() == State::Working);
    // a second request while one is in flight is refused
    assert(!f.auth.login("bob", "otherotherother"));
    assert(f.http.posts.size() == 1);
  }

  // ==== login 200: logged in, ticket kept, serial bumped, token stored ==== //
  {
    Fixture f;
    f.loggedIn(100);
    assert(f.auth.getTicket() == "T1");
    assert(f.auth.getTicketSerial() == 1);
    assert(f.store.token == TOKEN_A);
  }

  // ==== the token is saved before the new ticket is announced ==== //
  {
    Fixture f;
    int serialAtSave = -1;
    f.store.onSave = [&] { serialAtSave = f.auth.getTicketSerial(); };
    f.loggedIn(100);
    assert(serialAtSave == 0);
  }

  // ==== login failures: Error with a message, nothing stored ==== //
  {
    Fixture f;
    f.auth.login("alice", "wrongwrongwrong");
    f.answer(0, 401, "{\"error\":\"Invalid username or password\"}");
    assert(f.auth.getState() == State::Error);
    assert(f.auth.getErrorMesssage() == "Invalid username or password");
    assert(f.store.token.empty());

    // an Error is not a dead end: the player can try again
    assert(f.auth.login("alice", "hunter2hunter2"));
    f.answer(1, 200, loginBody("T1", TOKEN_A));
    assert(f.auth.getState() == State::LoggedIn);
  }
  {
    Fixture f;
    f.auth.login("alice", "hunter2hunter2");
    f.answer(0, 403, "{\"error\":\"Account banned\",\"reason\":\"cheating\",\"expires\":null}");
    assert(f.auth.getState() == State::Error);
    assert(f.auth.getErrorMesssage() == "Account banned");
    assert(f.store.token.empty());
  }
  {
    Fixture f;
    f.auth.login("alice", "hunter2hunter2");
    f.answer(0, 429, "{\"error\":\"Too many attempts\"}");
    assert(f.auth.getState() == State::Error);
    assert(f.auth.getErrorMesssage() == "Too many attempts");
  }
  {
    Fixture f;
    f.auth.login("alice", "hunter2hunter2");
    f.answer(0, 0);
    assert(f.auth.getState() == State::Error);
    assert(f.auth.getErrorMesssage() == "Can't reach server");
  }

  // ==== a 200 with an unreadable body is not a login ==== //
  {
    Fixture f;
    f.auth.login("alice", "hunter2hunter2");
    f.answer(0, 200, "not json");
    assert(f.auth.getState() == State::Error);
    assert(f.auth.getTicketSerial() == 0 && f.auth.getTicket().empty());
    assert(f.store.token.empty());
  }

  // ==== auto refresh: nothing until a minute before expiry, then /refresh ==== //
  {
    Fixture f;
    f.loggedIn(100);
    f.auth.update(100 + 3539);
    assert(f.http.posts.size() == 1);  // only the login so far
    f.auth.update(100 + 3540);
    assert(f.http.posts.size() == 2);
    assert(f.http.posts[1].url == URL + "/refresh");
    assert(parseRefreshToken(f.http.posts[1].body) == TOKEN_A);
    // a background refresh must not flip the UI to Working
    assert(f.auth.getState() == State::LoggedIn);
  }

  // ==== refresh 200: rotated token stored and used next time ==== //
  {
    Fixture f;
    f.loggedIn(100);
    f.auth.update(3640);
    f.answer(3641, 200, loginBody("T2", TOKEN_B));
    assert(f.auth.getTicket() == "T2" && f.auth.getTicketSerial() == 2);
    assert(f.store.token == TOKEN_B);
    assert(f.auth.getState() == State::LoggedIn);
    // the next refresh is a full lifetime later, with the new token
    f.auth.update(3641 + 3539);
    assert(f.http.posts.size() == 2);
    f.auth.update(3641 + 3540);
    assert(f.http.posts.size() == 3);
    assert(parseRefreshToken(f.http.posts[2].body) == TOKEN_B);
  }

  // ==== refresh 401: the token is dead, so log out and forget it ==== //
  {
    Fixture f;
    f.loggedIn(100);
    f.auth.update(3640);
    f.answer(3641, 401, "{\"error\":\"Invalid refresh token\"}");
    assert(f.auth.getState() == State::LoggedOut);
    assert(f.store.token.empty());
    f.auth.update(1000000);  // and it must not keep trying
    assert(f.http.posts.size() == 2);
  }

  // ==== refresh 403 (banned): token cleared, Error with the server's text ==== //
  {
    Fixture f;
    f.loggedIn(100);
    f.auth.update(3640);
    f.answer(3641, 403, "{\"error\":\"Account banned\",\"reason\":\"x\",\"expires\":null}");
    assert(f.auth.getState() == State::Error);
    assert(f.store.token.empty());
  }

  // ==== refresh with no answer or a 429: keep the token, stay logged in, retry in 30 s ==== //
  for (int status : {0, 429}) {
    Fixture f;
    f.loggedIn(100);
    f.auth.update(3640);
    f.answer(3641, status, status == 429 ? "{\"error\":\"Too many requests\"}" : "");
    assert(f.auth.getState() == State::LoggedIn);
    assert(f.store.token == TOKEN_A);
    f.auth.update(3641 + 29);
    assert(f.http.posts.size() == 2);
    f.auth.update(3641 + 30);
    assert(f.http.posts.size() == 3);
    assert(parseRefreshToken(f.http.posts[2].body) == TOKEN_A);
  }

  // ==== launch refresh: a saved token is tried, no token means no request ==== //
  {
    Fixture f;
    f.store.token = TOKEN_A;
    assert(f.auth.refresh());
    assert(f.http.posts.size() == 1 && f.http.posts[0].url == URL + "/refresh");
    assert(parseRefreshToken(f.http.posts[0].body) == TOKEN_A);
    assert(f.auth.getState() == State::Working);
    f.answer(5, 200, loginBody("T9", TOKEN_B));
    assert(f.auth.getState() == State::LoggedIn && f.auth.getTicket() == "T9");
    assert(f.store.token == TOKEN_B);
  }
  {
    Fixture f;
    assert(!f.auth.refresh());
    assert(f.http.posts.empty());
    assert(f.auth.getState() == State::LoggedOut);
  }
  {
    Fixture f;
    f.store.token = TOKEN_A;
    f.auth.refresh();
    f.answer(5, 401, "{\"error\":\"Invalid refresh token\"}");
    assert(f.auth.getState() == State::LoggedOut);
    assert(f.store.token.empty());
  }

  // ==== signup: /register, then /login with the same credentials, by itself ==== //
  {
    Fixture f;
    assert(f.auth.signup("alice", "hunter2hunter2"));
    assert(f.http.posts.size() == 1 && f.http.posts[0].url == URL + "/register");
    Creds c = parseCreds(f.http.posts[0].body);
    assert(c.name == "alice" && c.password == "hunter2hunter2");
    assert(f.auth.getState() == State::Working);

    f.answer(10, 201, "{\"ticket\":\"REG\"}");
    assert(f.http.posts.size() == 2 && f.http.posts[1].url == URL + "/login");
    c = parseCreds(f.http.posts[1].body);
    assert(c.name == "alice" && c.password == "hunter2hunter2");
    assert(f.auth.getState() == State::Working);  // still one continuous step
    assert(f.auth.getTicketSerial() == 0);        // the register ticket is not used

    f.answer(11, 200, loginBody("T1", TOKEN_A));
    assert(f.auth.getState() == State::LoggedIn && f.auth.getTicket() == "T1");
    assert(f.store.token == TOKEN_A);
  }

  // ==== signup failures: Error, and no /login is sent ==== //
  {
    Fixture f;
    f.auth.signup("alice", "hunter2hunter2");
    f.answer(10, 409, "{\"error\":\"Name already taken\"}");
    assert(f.auth.getState() == State::Error);
    assert(f.auth.getErrorMesssage() == "Name already taken");
    assert(f.http.posts.size() == 1);
  }
  {
    Fixture f;
    f.auth.signup("alice", "hunter2hunter2");
    f.answer(10, 0);
    assert(f.auth.getState() == State::Error);
    assert(f.auth.getErrorMesssage() == "Can't reach server");
    assert(f.http.posts.size() == 1);
  }
  {
    // the account exists but the chained login failed: not logged in, no second /register
    Fixture f;
    f.auth.signup("alice", "hunter2hunter2");
    f.answer(10, 201, "{\"ticket\":\"REG\"}");
    f.answer(11, 0);
    assert(f.auth.getState() == State::Error);
    assert(f.http.posts.size() == 2);
    assert(f.store.token.empty());
  }

  // ==== logout: local token gone first, server told, state LoggedOut ==== //
  {
    Fixture f;
    f.loggedIn(100);
    f.auth.logout();
    assert(f.auth.getState() == State::LoggedOut);
    assert(f.store.token.empty());
    assert(f.auth.getTicket().empty());
    assert(f.http.posts.size() == 2 && f.http.posts[1].url == URL + "/logout");
    assert(parseRefreshToken(f.http.posts[1].body) == TOKEN_A);
    f.answer(101, 204);
    assert(f.auth.getState() == State::LoggedOut);
    f.auth.update(1000000);  // logged out: no more refreshes
    assert(f.http.posts.size() == 2);
  }

  // ==== logout still works if the server never answers ==== //
  {
    Fixture f;
    f.loggedIn(100);
    f.auth.logout();
    f.answer(101, 0);
    assert(f.auth.getState() == State::LoggedOut);
    assert(f.store.token.empty());
  }

  // ==== logout while a refresh is in flight: the refresh result must not log you back in ==== //
  {
    Fixture f;
    f.loggedIn(100);
    f.auth.update(3640);  // refresh in flight
    f.auth.logout();
    assert(f.auth.getState() == State::LoggedOut && f.store.token.empty());
    assert(f.auth.getTicket().empty());
    f.answer(3641, 200, loginBody("T2", TOKEN_B));
    assert(f.auth.getState() == State::LoggedOut);
    assert(f.store.token.empty());
    assert(f.auth.getTicketSerial() == 1);
    // the server is still told, once the client is free
    f.auth.update(3642);
    f.auth.update(3643);
    bool told = false;
    for (const Post& p : f.http.posts) told = told || p.url == URL + "/logout";
    assert(told);
  }

  std::printf("auth tests passed\n");
  return 0;
}
