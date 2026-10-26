#include "adminPanel.hpp"

#include <raylib.h>

#include <cstdlib>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Auth/auth.hpp"
#include "Client/client.hpp"
#include "Input/inputState.hpp"
#include "Net/httpClient.hpp"
#include "Raylib/drawText.hpp"
#include "UI/Button/button.hpp"
#include "UI/Theme/theme.hpp"
#include "cereal/archives/json.hpp"
#include "env.hpp"

namespace {

// The server prefixes guest names; an account name can't contain a space.
bool isGuest(const std::string& name) { return name.starts_with("Guest "); }

// The longest reason /admin/ban accepts (banPlayerBody in adminRoutes.ts).
constexpr size_t MAX_REASON_LENGTH = 64;

// The longest ban /admin/ban accepts, one year (banPlayerBody in
// adminRoutes.ts).
constexpr int MAX_BAN_HOURS = 8760;

}  // namespace

// Its own HttpClient: Auth's is busy during a refresh and would drop the post.
AdminPanel::AdminPanel(Auth& auth, Client& client, Input& input,
                       std::function<void(void)> exit)
    : auth(auth),
      client(client),
      input(input),
      http(makeHttpClient()),
      reasonBox(banReason, 0.0f, false, ButtonStyles::Secondary, input,
                "Reason"),
      hoursBox(banHours, 0.0f, false, ButtonStyles::Secondary, input, "Hours"),
      unbanBox(unbanName,
               {10.0f, (float)GetScreenHeight() - (BUTTON_HEIGHT * 0.75f) - 10,
                BUTTON_WIDTH, BUTTON_HEIGHT * 0.75f},
               30, false, ButtonStyles::Secondary, input, "Unban"),
      exit(exit) {}

std::vector<OnlinePlayer> AdminPanel::getPlayers(bool guestsShown) {
  std::vector<OnlinePlayer> players = client.getPlayers();
  std::erase_if(players, [&](OnlinePlayer player) {
    if (!player.name.has_value()) return true;
    if (guestsShown) {
      return false;
    } else {
      return isGuest(*player.name);
    }
  });
  return players;
}

constexpr float STATUS_MESSAGE_TIME = 4.0f;

// Runs every frame, open or not, so a response is never left in the client.
void AdminPanel::update() {
  if (IsWindowResized()) {
    unbanBox.setRec({10.0f, (float)GetScreenHeight() - BUTTON_HEIGHT - 10,
                     BUTTON_WIDTH, BUTTON_HEIGHT});
  }
  std::optional<HttpResponse> res = http->poll();
  if (!res.has_value()) return;
  switch (res->status) {
    case 204: {
      if (request == AdminRequest::Ban) {
        // The account is banned now, so end their live session too.
        client.banPlayerMessage(banTargetName, banReason);
        statusMessage = "Successfully banned player";
      } else {
        statusMessage = "Successfully unbanned player";
      }
      banTargetId = std::nullopt;
      banTargetName.clear();
      banReason.clear();
      banHours.clear();
      statusTime = STATUS_MESSAGE_TIME;
      state = AdminPanelState::Idle;
      break;
    }
    case 401: {
      auth.refresh();
      statusTime = STATUS_MESSAGE_TIME;
      statusMessage = "Error occurred, try again";
      state = AdminPanelState::Failed;
      break;
    }
    case 403: {
      statusTime = STATUS_MESSAGE_TIME;
      statusMessage = "Not an admin";
      state = AdminPanelState::Failed;
      break;
    }
    case 404: {
      statusTime = STATUS_MESSAGE_TIME;
      statusMessage = "No such player";
      state = AdminPanelState::Failed;
      break;
    }
    case 400: {
      statusTime = STATUS_MESSAGE_TIME;
      statusMessage = "Invalid";
      state = AdminPanelState::Failed;
      break;
    }
    case 0: {
      statusTime = STATUS_MESSAGE_TIME;
      statusMessage = "Can't reach server";
      state = AdminPanelState::Failed;
      break;
    }
    default: {
      statusTime = STATUS_MESSAGE_TIME;
      statusMessage = "Unknown error. " + std::to_string(res->status);
      state = AdminPanelState::Failed;
      break;
    }
  }
}

void AdminPanel::frame(float dt) {
  std::vector<OnlinePlayer> players = getPlayers(showGuests);

  DrawRectangleRec({0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                   ColorAlpha(Theme::Ground, 100));

  constexpr float STARTING_Y = 100;
  constexpr float ROW_HEIGHT = 80;
  int row{0};  // starts from 0

  DrawTextFont(
      "Admin panel",
      GetScreenWidth() / 2 - MeasureTextFont("Admin panel", 50) / 2 + 5, 25 + 5,
      50, Theme::Blue);
  DrawTextFont("Admin panel",
               GetScreenWidth() / 2 - MeasureTextFont("Admin panel", 50) / 2,
               25, 50, Theme::Text);

  if (players.empty()) {
    DrawTextFont("No other players online",
                 GetScreenWidth() / 2 -
                     MeasureTextFont("No other players online", 30) / 2,
                 STARTING_Y + 20, 30, Theme::Text);
  }

  std::string targetName;  // empty if the selected player is gone

  for (auto& p : players) {
    const float y = STARTING_Y + (row * ROW_HEIGHT);
    const Rectangle rowRec = {100, y, GetScreenWidth() - 200.0f, ROW_HEIGHT};
    DrawRectangleRec(rowRec, row % 2 == 0 ? Theme::Blue : Theme::BlueDark);

    // name
    DrawTextFont(p.name->c_str(), 120, y + (ROW_HEIGHT / 2) - 15, 30,
                 Theme::Text);

    if (isGuest(*p.name)) {
      // a guest has no account, so there is nothing to ban
      DrawTextFont("guest", 120 + MeasureTextFont(p.name->c_str(), 30) + 15,
                   y + (ROW_HEIGHT / 2) - 8, 20, Theme::TextMuted);
    } else {
      const Rectangle banRec = {rowRec.x + rowRec.width - 140, y + 15, 120, 50};
      Button ban(
          [this, id = p.id, name = *p.name] {
            if (banTargetId != id) {
              banReason.clear();
              banHours.clear();
            }
            banTargetId = id;
            banTargetName = name;
          },
          "Ban", banRec, 25, ButtonStyles::Multi);
      ban.frame(input, input.pressed(Action::Click));
    }

    if (banTargetId == p.id) {
      DrawRectangleLinesEx(rowRec, 3, Theme::Red);
      targetName = *p.name;
    }

    row++;
  }

  auto drawText = [&](std::string text, Color color = Theme::Text,
                      Color secondary = Theme::Blue) {
    DrawTextFont(
        text.c_str(),
        GetScreenWidth() / 2 - MeasureTextFont(text.c_str(), 50) / 2 + 5,
        GetScreenHeight() / 2 - 25 + 5, 50, secondary);
    DrawTextFont(text.c_str(),
                 (GetScreenWidth() - MeasureTextFont(text.c_str(), 50)) / 2,
                 GetScreenHeight() / 2 - 25, 50, color);
  };
  statusTime -= dt;
  if (state == AdminPanelState::Sending) {
    drawText("Loading...");
  } else if (!statusMessage.empty()) {
    if (statusTime <= 0) {
      statusMessage.clear();
    } else {
      if (state == AdminPanelState::Failed) {
        drawText(statusMessage, Theme::Red, Theme::RedDark);
      } else {
        drawText(statusMessage);
      }
    }
  }

  unbanBox.frame();
  Button unbanButton(
      [&]() {
        if (state == AdminPanelState::Sending) return;
        if (unbanName.empty()) {
          statusMessage = "Name required";
          state = AdminPanelState::Failed;  // to make the status red
          statusTime = STATUS_MESSAGE_TIME;
          return;
        }
        UnbanPlayerRequest req{unbanName};
        std::stringstream is;
        {
          cereal::JSONOutputArchive ar(is);
          req.serialize(ar);
        }
        std::string body = is.str();
        http->post(env::AUTH_URL + std::string("/admin/unban"), body,
                   "Bearer " + auth.getTicket());
        state = AdminPanelState::Sending;
        request = AdminRequest::Unban;
        unbanName.clear();
      },
      "Unban",
      {470.0f, (float)GetScreenHeight() - (BUTTON_HEIGHT * 0.75f) - 10,
       BUTTON_WIDTH * 0.5f, BUTTON_HEIGHT * 0.75f},
      30, ButtonStyles::Multi);
  unbanButton.frame(input, input.pressed(Action::Click));

  Button exitButton(exit, "<", {10, 10, 50, 50}, 50);

  exitButton.frame(input, input.pressed(Action::Click));

  if (!banTargetId) return;

  // The chosen player left, so there is no one to ban any more.
  if (targetName.empty()) {
    banTargetId.reset();
    banTargetName.clear();
    banReason.clear();
    banHours.clear();
    return;
  }

  drawBanForm(targetName);
}

void AdminPanel::drawBanForm(const std::string& name) {
  // One column, centred, GAP apart: label, reason, hours, then Cancel |
  // Confirm.
  constexpr float GAP = 10;
  constexpr float LABEL_HEIGHT = 30;
  constexpr float BOTTOM_MARGIN = 40;
  constexpr float FORM_HEIGHT = LABEL_HEIGHT + 3 * BUTTON_HEIGHT + 3 * GAP;

  const float labelY = GetScreenHeight() - BOTTOM_MARGIN - FORM_HEIGHT;
  const float reasonY = labelY + LABEL_HEIGHT + GAP;
  const float hoursY = reasonY + BUTTON_HEIGHT + GAP;
  const float buttonsY = hoursY + BUTTON_HEIGHT + GAP;
  const float left = (GetScreenWidth() - BUTTON_WIDTH) / 2.0f;
  const float halfWide = (BUTTON_WIDTH - GAP) / 2.0f;

  const std::string label = "Reason for banning " + name;
  DrawTextFont(label.c_str(),
               (GetScreenWidth() - MeasureTextFont(label.c_str(), 30)) / 2,
               labelY, 30, Theme::Text);

  reasonBox.setYPos(reasonY);
  reasonBox.frame();
  hoursBox.setYPos(hoursY);
  hoursBox.frame();
  if (banReason.size() > MAX_REASON_LENGTH) banReason.resize(MAX_REASON_LENGTH);

  const Rectangle cancelRec = {left, buttonsY, halfWide, BUTTON_HEIGHT};
  const Rectangle confirmRec = {left + halfWide + GAP, buttonsY, halfWide,
                                BUTTON_HEIGHT};
  Button cancel(
      [this] {
        banTargetId.reset();
        banTargetName.clear();
        banReason.clear();
        banHours.clear();
      },
      "Cancel", cancelRec, 25, ButtonStyles::Secondary);
  cancel.frame(input, input.pressed(Action::Click));

  Button confirm(
      [this] {
        if (state != AdminPanelState::Sending && banTargetId.has_value()) {
          // the route requires a reason of at least one character
          if (banReason.empty()) {
            statusTime = STATUS_MESSAGE_TIME;
            statusMessage = "Reason required";
            state = AdminPanelState::Failed;
            return;
          }
          // empty or not a number reads as 0, which is a permanent ban
          const int hours = atoi(banHours.c_str());
          if (hours < 0 || hours > MAX_BAN_HOURS) {
            statusTime = STATUS_MESSAGE_TIME;
            statusMessage = "Hours: 1 - 8760, or empty for permanent";
            state = AdminPanelState::Failed;
            return;
          }
          std::stringstream is;
          BanPlayerRequest req{banTargetName, banReason, hours};
          {
            cereal::JSONOutputArchive ar(is);
            req.serialize(ar);
          }
          std::string jsonBody = is.str();
          http->post(std::string(env::AUTH_URL) + "/admin/ban", jsonBody,
                     "Bearer " + auth.getTicket());
          state = AdminPanelState::Sending;
          request = AdminRequest::Ban;
        }
      },
      "Confirm", confirmRec, 25, ButtonStyles::Secondary);
  confirm.frame(input, input.pressed(Action::Click));
}