#include <raylib.h>

#include "AccountMenu/accountMenu.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <iostream>
#include <memory>

#include "AssetManager/manager.hpp"
#include "Game/game.hpp"
#include "GameState/gameState.hpp"
#include "Home/home.hpp"
#include "Input/input.hpp"
#include "Input/inputSource.hpp"
#include "Net/httpClient.hpp"
#include "Protocol/protocol.hpp"
#include "Settings/settings.hpp"
#include "env.hpp"

// ==== state ==== //

// File scope so update() and destroy() can reach what ready() built.
static std::unique_ptr<AssetManager> assets;
static std::unique_ptr<Game> game;
static std::unique_ptr<Home> home;
static std::unique_ptr<Settings> settings;
static std::unique_ptr<Input> input;
static std::unique_ptr<HttpClient> httpClient;
static std::unique_ptr<AccountMenu> accounts;

// ==== lifecycle ==== //

static void ready() {
#ifndef __EMSCRIPTEN__
  // Native: asset paths are relative to the binary, not the shell.
  ChangeDirectory(GetApplicationDirectory());
#endif

  // Suppress raylib's unnecessary logging levels.
  SetTraceLogLevel(LOG_WARNING);

  std::cout << "Game version: " << env::VERSION << "\n";
  std::cout << "protocol version: " << proto::PROTOCOL_VERSION << "\n";

#ifndef __EMSCRIPTEN__

  // -------------------------------------------------------------------------
  // Native
  //
  // Preserve the existing native behaviour:
  // - Resizable
  // - HiDPI
  // - VSync
  // - Resize to the current monitor
  // -------------------------------------------------------------------------
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);

#else

  // -------------------------------------------------------------------------
  // Web / Emscripten
  //
  // Let the browser/HTML canvas control the display size.
  // Avoid FLAG_WINDOW_HIGHDPI and FLAG_VSYNC_HINT here because browser
  // compositors already control presentation timing, and HiDPI can result
  // in a much larger WebGL framebuffer than the visible canvas.
  // -------------------------------------------------------------------------
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);

#endif

  std::cout << "Create window\n";

  InitWindow(1280, 720, ("Sandbox Network - " + env::VERSION).c_str());

#if !defined(__EMSCRIPTEN__) && !defined(PLATFORM_IOS) && \
    !defined(PLATFORM_TVOS)

  // Native: preserve the existing fullscreen-ish monitor-sized behaviour.
  int mw = GetMonitorWidth(GetCurrentMonitor());
  int mh = GetMonitorHeight(GetCurrentMonitor());

  SetWindowSize(mw, mh);
  SetWindowPosition(0, 0);

#endif

  SetExitKey(KEY_F12);

#ifdef __EMSCRIPTEN__

  // Do not let raylib introduce an additional frame-rate cap.
  // The browser's requestAnimationFrame/main loop controls presentation.
  SetTargetFPS(0);

#else

  // The settings slider changes this later; FLAG_VSYNC_HINT paces presentation
  // too.
  SetTargetFPS(GameState::shared().getTargetFps());

#endif

  assets = std::make_unique<AssetManager>();
  input = std::make_unique<Input>(makeInputSource(), *assets);
  game = std::make_unique<Game>(*assets, *input);
  home = std::make_unique<Home>(*input);
  settings = std::make_unique<Settings>(*input);
  httpClient = makeHttpClient();
  accounts = std::make_unique<AccountMenu>(*input, *httpClient);

  GameState::shared().setMainFont(assets->get(Fon::PressStart2P));

  input->setMouseLook(false);
}

static void update() {
  const static GameState& gameState = GameState::shared();
  static MenuState prevMenu = MenuState::Home;
  const MenuState menu = gameState.getMenu();
  input->update();

  if (menu == MenuState::Playing) {
    game->frame(prevMenu != menu);
  } else if (menu == MenuState::Home) {
    home->frame();
  } else if (menu == MenuState::Settings) {
    settings->frame();
  } else if (menu == MenuState::Login) {
    accounts->frame();
  }

  prevMenu = menu;
}

static void destroy() {
  // Reverse order, and before CloseWindow() destroys the GL context.
  accounts.reset();
  httpClient.reset();
  settings.reset();
  home.reset();
  game.reset();
  input.reset();
  assets.reset();

  CloseWindow();
}

// ==== entry point ==== //

#if defined(PLATFORM_IOS) || defined(PLATFORM_TVOS)

// iOS/tvOS own the main loop and call these three (see rcore_ios.c,
// rcore_tvos.c); C linkage so the C side finds them.
extern "C" {
void ios_ready() { ready(); }

void ios_update() { update(); }

void ios_destroy() { destroy(); }
}

#else

int main() {
  ready();

#ifdef __EMSCRIPTEN__
  // The browser owns the loop (never returns); a blocking loop needs ASYNCIFY.
  emscripten_set_main_loop(update, 0, 1);
#else
  while (!WindowShouldClose() && !GameState::shared().shouldClose()) {
    update();
  }
#endif

  destroy();
  return 0;
}

#endif

#ifdef __EMSCRIPTEN__

extern "C" {

void resize(int w, int h) { SetWindowSize(w, h); }
}

#endif
