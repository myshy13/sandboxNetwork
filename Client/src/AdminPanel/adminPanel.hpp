#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Auth/auth.hpp"
#include "Client/client.hpp"
#include "Input/input.hpp"
#include "Net/httpClient.hpp"
#include "UI/TextBox/textbox.hpp"
#include "cereal/cereal.hpp"

enum class AdminPanelState { Idle, Sending, Failed };

struct BanPlayerRequest {
  std::string name;
  std::string reason;
  int hours{0};  // 1 - 8,760; 0 leaves the key out, which the route reads as permanent

  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(name), CEREAL_NVP(reason));
    if (hours > 0) ar(CEREAL_NVP(hours));
  }
};

struct UnbanPlayerRequest {
  std::string name;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(name));
  }
};

class AdminPanel {
 private:
  Auth& auth;
  Client& client;
  Input& input;

  std::unique_ptr<HttpClient> http;

  bool showGuests{true};

  std::optional<int> banTargetId;  // player whose Ban button was clicked
  std::string banTargetName;       // their account name, what /admin/ban takes
  std::string banReason;           // what is typed into reasonBox
  std::string banHours;            // use atoi on request
  TextBox reasonBox;               // after banReason, which it points at
  TextBox hoursBox;                // hours of player ban

  std::string unbanName;
  TextBox unbanBox;

  // HTTP State
  AdminPanelState state{AdminPanelState::Idle};
  std::string statusMessage;
  std::string successMessage;  // shown on a 204, set by whichever request is in flight
  float statusTime{0.0f};

  std::vector<OnlinePlayer> getPlayers(bool guests = false);
  void drawBanForm(const std::string& name);

 public:
  AdminPanel(Auth& auth, Client& client, Input& input);
  void update();         // polls the ban request; call every frame
  void frame(float dt);  // draws the panel; call only while open
};