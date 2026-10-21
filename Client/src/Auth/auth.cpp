#include "auth.hpp"

#include <cereal/archives/json.hpp>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

#include "Auth/tokenStore.hpp"
#include "Net/httpClient.hpp"
#include "cereal/cereal.hpp"
#include "cereal/details/helpers.hpp"
#include "env.hpp"

Auth::Auth(TokenStore& tokenStore, HttpClient& httpClient)
    : tokenStore(tokenStore), http(httpClient) {}

namespace {

// Flat {"refreshToken": "..."} body, shared by /refresh and /logout.
std::string tokenBody(const std::string& refreshToken) {
  std::stringstream ss;
  {
    cereal::JSONOutputArchive ar(ss);
    ar(CEREAL_NVP(refreshToken));
  }  // the archive flushes when it goes out of scope
  return ss.str();
}

// False when the body isn't the JSON we expected; out is then unusable.
template <class T>
bool parseJson(const std::string& body, T& out) {
  try {
    std::istringstream iss(body);
    cereal::JSONInputArchive ar(iss);
    out.serialize(
        ar);  // not ar(out): that expects the fields nested under "value0"
    return true;
  } catch (const cereal::Exception&) {
    return false;
  }
}

// The server's own error text, or the fallback if the body has none.
std::string errorFrom(const std::string& body, const std::string& fallback) {
  ErrorResponse res;
  if (!parseJson(body, res) || res.error.empty()) return fallback;
  return res.error;
}

// The ban reason from a 403 body; empty when it is missing, null or unreadable. Read with RapidJSON directly
// because cereal throws on the nulls the server sends (no reason, or expires: null for a permanent ban).
std::string banReasonFrom(const std::string& body) {
  rapidjson::Document doc;
  doc.Parse(body.c_str());
  if (doc.HasParseError() || !doc.IsObject()) return "";

  const auto reason = doc.FindMember("reason");
  if (reason == doc.MemberEnd() || !reason->value.IsString()) return "";
  return reason->value.GetString();
}

}  // namespace

void Auth::update(int64_t now) {
  prevNow = now;

  if (pending != Pending::None) {
    std::optional<HttpResponse> res = http.poll();
    if (res.has_value()) {
      handleResponse(res.value());
    }
  } else if (!pendingLogoutToken.empty()) {
    // best effort: one try, the token is dropped whatever the answer
    sendRequest(Pending::Logout, "/logout", tokenBody(pendingLogoutToken));
    pendingLogoutToken.clear();
  } else if (now >= nextRefreshAt && state == State::LoggedIn) {
    sendRequest(Pending::Refresh, "/refresh", tokenBody(refreshToken));
  }
}

// bool whether the request was sent or not
bool Auth::sendRequest(const Pending p, const std::string& path,
                       const std::string& body) {
  if (p == Pending::None || pending != Pending::None) return false;

  http.post(env::AUTH_URL + path, body);
  pending = p;
  return true;
}

constexpr int TICKET_LIFETIME = 3600;
constexpr int REFRESH_MARGIN = 60;  // double the servers' for safety
constexpr int RETRY_DELAY = 30;

void Auth::handleResponse(HttpResponse res) {
  const Pending type = pending;
  pending = Pending::None;

  // Copied out and wiped for every reply: the password never outlives the
  // register reply.
  const std::string name = pendingName;
  const std::string password = pendingPassword;
  pendingName.clear();
  pendingPassword.clear();

  // Logout is best effort and the server always answers 204, so there is
  // nothing to read.
  if (type == Pending::Logout) return;

  // A failure that is not a ban resets banReason, so an old reason never shows under a new error.
  auto fail = [&](const std::string& message, const std::string& reason = "") {
    state = State::Error;
    errorMessage = message;
    banReason = reason;
  };

  // ==== success ==== //
  if (res.status >= 200 && res.status < 300) {
    if (type == Pending::Register) {
      RegisterResponse reg;
      if (!parseJson(res.body, reg))
        return fail("Unexpected reply from server");
      // The register ticket is ignored: /login is the one path into LoggedIn.
      if (!login(name, password)) fail("Account created, please log in");
      return;
    }

    LoginResponse reply;
    if (!parseJson(res.body, reply))
      return fail("Unexpected reply from server");
    // Signed out while this refresh was in flight: don't log back in, drop the
    // new token.
    if (state == State::LoggedOut) {
      pendingLogoutToken = reply.refreshToken;
      return;
    }
    // Saved before the ticket is announced, so a crash can't strand the
    // account.
    tokenStore.save(reply.refreshToken);
    refreshToken = reply.refreshToken;
    ticket = reply.ticket;
    ticketSerial++;
    nextRefreshAt = prevNow + TICKET_LIFETIME - REFRESH_MARGIN;
    errorMessage.clear();
    banReason.clear();
    state = State::LoggedIn;
    return;
  }

  // ==== a refresh that got no answer keeps its token and tries again ==== //
  const bool transient =
      res.status == 0 || res.status == 429 || res.status >= 500;
  if (type == Pending::Refresh && transient) {
    if (state == State::LoggedIn) {
      nextRefreshAt = prevNow + RETRY_DELAY;
    } else {
      fail("Can't reach server");  // launch refresh: token kept, show the login
                                   // screen
    }
    return;
  }

  // ==== failures ==== //
  switch (res.status) {
    case 401:
      if (type == Pending::Login) return fail("Invalid username or password");
      if (type == Pending::Refresh) {
        tokenStore.clear();
        refreshToken.clear();
        state = State::LoggedOut;
        return;
      }
      break;
    case 403: {
      if (type == Pending::Refresh) {
        tokenStore.clear();
        refreshToken.clear();
      }
      return fail(errorFrom(res.body, "Account banned"), banReasonFrom(res.body));
    }
    case 429:
      return fail(errorFrom(res.body, "Too many attempts"));
    case 0:
      return fail("Can't reach server");
  }
  // Anything else (register 400 / 409, a 5xx on login): show the server's text.
  fail(errorFrom(res.body, "Unexpected server error"));
}

// used for login and register
struct AccountRequest {
  std::string name;
  std::string password;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(name), CEREAL_NVP(password));
  }
};

// used for refresh and logout
struct RefreshRequest {
  std::string refreshToken;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(refreshToken));
  }
};

bool Auth::login(const std::string& username, const std::string& password) {
  state = State::Working;

  AccountRequest req = {username, password};
  std::stringstream iss;
  try {
    cereal::JSONOutputArchive ar(iss);
    req.serialize(ar);
  } catch (cereal::Exception& e) {
    return false;
  }
  std::string jsonBody = iss.str();
  return sendRequest(Pending::Login, "/login", jsonBody);
}

bool Auth::signup(const std::string& username, const std::string& password) {
  state = State::Working;

  pendingName = username;
  pendingPassword = password;

  AccountRequest req = {username, password};
  std::stringstream iss;
  try {
    cereal::JSONOutputArchive ar(iss);
    req.serialize(ar);
  } catch (cereal::Exception& e) {
    return false;
  }
  std::string jsonBody = iss.str();
  return sendRequest(Pending::Register, "/register", jsonBody);
}

void Auth::logout() {
  const std::string token = refreshToken;

  refreshToken.clear();
  ticket.clear();
  tokenStore.clear();
  state = State::LoggedOut;

  // Nothing to tell the server if there was no session.
  if (token.empty()) return;
  // Busy (a refresh is in flight): update() sends it once the client is free.
  if (!sendRequest(Pending::Logout, "/logout", tokenBody(token))) {
    pendingLogoutToken = token;
  }
}

bool Auth::refresh() {
  refreshToken = tokenStore.load();
  if (refreshToken.empty()) return false;

  RefreshRequest req;
  req.refreshToken = refreshToken;
  std::stringstream iss;
  {
    cereal::JSONOutputArchive ar(iss);
    req.serialize(ar);
  }
  std::string body = iss.str();

  if (sendRequest(Pending::Refresh, "/refresh", body)) {
    state = State::Working;
    return true;
  } else {
    return false;
  }
}