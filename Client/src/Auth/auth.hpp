#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "Auth/tokenStore.hpp"
#include "Net/httpClient.hpp"
#include "cereal/cereal.hpp"

enum class State { LoggedOut, Working, LoggedIn, Error };
enum class Pending { None, Login, Register, Refresh, Logout };

struct LoginResponse {
  std::string ticket;  // base64
  std::string refreshToken;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(ticket), CEREAL_NVP(refreshToken));
  }
};

struct RegisterResponse {
  std::string ticket;  // base64

  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(ticket));
  }
};

struct ErrorResponse {
  std::string error;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(error));
  }
};

class Auth {
 private:
  State state{State::LoggedOut};
  Pending pending{Pending::None};
  TokenStore& tokenStore;
  HttpClient& http;

  std::string ticket{""};  // base64
  int ticketSerial{0};
  std::string refreshToken;  // memory copy
  int64_t nextRefreshAt{0};
  int64_t prevNow{0};

  std::string pendingName;
  std::string pendingPassword;
  std::string pendingLogoutToken;  // sent from update() once the client is free

  std::string errorMessage;

  bool sendRequest(const Pending p, const std::string& path,
                   const std::string& body);
  void handleResponse(HttpResponse res);

 public:
  Auth(TokenStore& tokenStore, HttpClient& httpClient);

  void update(int64_t now);
  bool login(const std::string& username, const std::string& password);
  bool signup(const std::string& username, const std::string& password);
  bool refresh();
  void logout();

  State getState() { return state; }

  const std::string& getTicket() { return ticket; }

  int getTicketSerial() { return ticketSerial; }

  std::string getErrorMesssage() { return errorMessage; }
};