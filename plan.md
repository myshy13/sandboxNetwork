# Plans

## Block registry (so a new block is a row, not a hunt)

Goal: the enum stays each block's identity (one byte, on the wire and in saves); everything else about a block is a
lookup on it. Today `== BlockType::Water` is tested about 17 times across the renderer, world, server and `FluidSim`,
and one `solid` flag has to mean both "players collide" and "hides the face behind it". Stage 1 is the cheap, mechanical
part. Per-block behaviour hooks wait for a second block that has logic (sand, lava), so they are designed from two real
cases, not from water alone. `Object` stays small: ~843,000 live, so no per-block pointers or virtuals.

### Stage 2 (when a second block has logic)

A server-side table of optional function pointers per block (`onTick`, `onNeighbourChange`, `onPlace`, `onBreak`,
`nullptr` = nothing); water's flow becomes its entry. A client-side shape per block (cube, or water's variable height).

## Day time and lighting

What's left:

- [-] Cache the map: redraw only when the snapped centre moves, the sun has moved a few degrees, or a chunk went
  dirty (the pass costs ~6 ms every frame today) **Skipped:** Unnecessary
- [x] Snap the map centre on a coarse world grid (`SHADOW_SNAP` 20 units on x and z, map padded by `SHADOW_SNAP * sqrt 2`),
      so the grid no longer turns with the sun. The once-a-second hop is much less noticeable but not gone; left as is
      (caching would cut it further)
- [x] A shadow radius of its own (`GameState::getShadowRadius()`, default 200 units, 50 to 400 from a slider on the
      settings screen, capped by the render distance): sharper map, ~6x fewer faces at the default; the shaders fade the
      shadow out over the map's outer tenth
- [ ] A depth-only shader for the pass (the lighting shader does per-pixel work that is thrown away)
- [x] Slope-scaled bias in both shaders (`0.0005 * tan(angle to the normal)`, clamped 0.0001 to 0.005); checked in the
      afternoon, the stripes are gone. Tune by eye if it changes (stripes = too small, floating shadows = too big)
- [ ] Night: skip the pass while the sun is below the horizon
- [ ] Web: check the depth-texture extension and `glsl100` in a browser once

Known limits: a low sun squashes the covered area into a thin ellipse of the map, so shadows blur along the sun's
direction; unloaded chunks count as open air, so shadows can pop in as chunks load; water casts none.

Later: ambient occlusion (darken corners where blocks meet, same neighbour-lookup idea at chunk rebuild).

Maybe later: cascaded shadow maps (2-3 maps of growing size around the player; the shader picks the smallest that
contains the pixel). Not wanted yet; do it only after the single map is snapped, cached and has its own radius.

## Accounts (`sandboxNetworkAuth`)

Goal: an always-on server (AWS, maybe an Oracle VPS later) needs a banlist, real names instead of "player \<n>", and a
persistent identity. Not started; branch `feature/accounts`, merged by PR (it changes who may play and build).

Decisions:

- Login is username + password only. No OAuth, no email in v1 (email recovery is a later item).
- The auth service is its own program, `sandboxNetworkAuth`, written in **Node.js (not C++)** with **Fastify** (schema
  validation of every body, `@fastify/rate-limit`, `@fastify/helmet`; picked over Express on security grounds).
- Accounts live in a SQLite file owned by the auth service (`better-sqlite3`). Passwords are hashed with Argon2id (`argon2`).
- The password only ever travels over HTTPS to the auth service (Caddy + Let's Encrypt, or an ALB + ACM), never over ENet.
- After login the client holds an **Ed25519 signed ticket** `{accountId, name, isAdmin, exp}`. Auth holds the private
  key, game servers hold only the public key, so a stolen game server cannot mint tickets. A ticket lasts 1 hour; the client
  asks for a new one a minute before it expires.
- The game server verifies the ticket once, in `clientHandshake`, and keeps `accountId` on the `Player`. Identity still
  comes from the connection, never from a later payload.
- Bans sit on the account (`banned`, reason, optional expiry). Registration is rate-limited per IP so a ban sticks. A ban also deletes all the account's refresh tokens (`Db.setBan`, one transaction); tickets already issued stay valid until `exp`.
- Guests are configurable per game server: `--allow-guests` plus a few bools in `Server/src/env.hpp` for what a guest
  may do (build, chat, ...). Guests are named "Guest \<n>".
- Some accounts are admins (a flag in the ticket); a CLI script on the auth box does bans until something better is needed.
- Client: a Login / Register / Guest screen on first launch, and a small "Sign out" button on the Home screen.

Stages (each ends in something runnable). Route-level backlog for stage 1: `Auth/routes-backlog.md`.

1. [x] Auth service on its own (`Auth/`, Node, outside the CMake build): `db.js` (accounts table, prepared statements
       only), `passwords.js` (Argon2id; run a dummy verify for unknown names so timing doesn't reveal which exist),
       `tickets.js` (sign a fixed byte layout, not JSON, so C++ can parse the same bytes), routes `/register` `/login`
       `/refresh`, `ban.js` / `unban.js`, one `node --test` file. Name rules start strict: ASCII `[A-Za-z0-9_]`, 3-16
       chars (loosening later is easy, tightening is not).
2. [x] Game server verifies tickets: `ticket` string on `clientHandshake` (empty = guest), bump `PROTOCOL_VERSION`; a
       verify-only Ed25519 lib via `FetchContent` (monocypher or libsodium) behind a `TicketVerifier` class in
       `Server/src/Auth/`; `--allow-guests` and `--auth-pubkey <file>` args; guest checks in `PlaceObject` and chat;
       expiry checked on a timer in `Server::tick` with a small grace window; same account joining twice kicks the old
       session; tests in `cd Server && make test` (valid, tampered, expired, wrong key, guest allowed/denied).
3. [ ] Client login flow (branch `feature/client-login`): HTTP client behind an interface, one class per backend (native
       HTTPS lib, Emscripten fetch on web), no `#ifdef` in game code; `MenuState::Login` screen from the existing
       `Button`s and Input layer; a longer-lived refresh token stored per platform (file native, `localStorage` web),
       never the password; Sign out button; refresh message a minute before expiry. tvOS/iOS have no text entry, so they
       stay guest-only at first.
   - [x] 3a. `HttpClient` interface (`Client/src/Net/httpClient.hpp`) + native backend (`httpClient_native.cpp`: worker
         thread, cpp-httplib + Mbed TLS via FetchContent, bundled `assets/cacert.pem`; the OS root stores are disabled so
         only that file is trusted). Checked against the local auth service (200 / 401 / status 0 = no connection).
   - [x] 3b. `AccountMenu` (Login screen): `TextBox` widget, Login / Signup buttons, takes an `HttpClient&` owned in
         `main.cpp`. `submitLogin` / `submitSignup` are still empty (3f).
   - [x] 3c. `TokenStore` (`Client/src/Auth/`): interface `load / save / clear` + `makeTokenStore()`; `TokenStoreFile`
         writes `~/.sandboxNetwork/token` via a temp file and rename (owner-only), `load` accepts only 43 base64url
         characters, a missing or corrupt file is "no token". Excluded from the web build.
   - [ ] 3d. Test `TokenStoreFile` against a temp directory (needs a way to inject the folder, e.g. a constructor path
         argument defaulting to the home folder): save then load round-trips, load of garbage / wrong length / missing
         file gives empty, clear then load gives empty, a failed save leaves the old token. Standalone `make test`,
         no raylib or network, like `Shared/Protocol`.
   - [ ] 3e. `Auth` class (`Client/src/Auth/`, non-blocking, no threads of its own): takes `HttpClient&` and
         `TokenStore&`. Public: `login / signup / refresh / logout`, `update(now)` once per frame, a state enum
         (LoggedOut, Working, LoggedIn, Error), the current ticket, and `ticketSerial` (bumped on every new ticket so
         the client knows to send `TicketRefresh`).
         - Constants (`TICKET_LIFETIME` 3600 s, `REFRESH_MARGIN` 60 s, `RETRY_DELAY` 30 s, `AUTH_URL`) in
           `Client/src/env.hpp` and `env.example.hpp`.
         - Save the new refresh token before using the response, so a crash can't strand the account.
         - Refresh: 401 clears the token (logged out); status 0 (no network) keeps it and retries after `RETRY_DELAY`.
         - Logout clears the local token first, then tells the server (best effort).
         - Signup chains `/login` (register returns no refresh token).
         - Tests with a fake `HttpClient` and a fake `TokenStore`: each route's 200 / 401 / 0, the retry timer, and the
           token being saved before the ticket is used.
   - [ ] 3f. Wire it in: `submitLogin` / `submitSignup` call `Auth`; `Client::connect(ticket)` puts the ticket in
         `clientHandshake`; on start, a saved token tries `refresh` before showing the Login screen; `TicketRefresh`
         is sent whenever `ticketSerial` changes; Sign out button on Home; guest play stays one click away.
   - [ ] 3g. Web backend: `HttpClient` over `emscripten_fetch`, CORS on the auth service for the web origin,
         `TokenStore` over `localStorage` (also fixes the web link error from `makeTokenStore` being undefined).
   - [ ] 3h. iOS / tvOS: guest-only (no text entry). Check Mbed TLS and cpp-httplib build there; if not, stub the
         `HttpClient` and `TokenStore` with no-ops.
   - Known limit: two clients on one machine share the token file, so one failed refresh clears the other's token.
     Fix later with a profile name in the path.
4. [ ] Deployment: HTTPS in front of the auth service, systemd unit, key file permissions, SQLite backups (S3 later), game
       server flags. Later: email + password reset, revocation list pushed to game servers, admin web UI.

Security checklist: Argon2id with per-password salt and the same error for unknown name vs wrong password; rate limits on
login (IP + name) and register (IP); parameterised SQL, body schemas and a size cap; the private key never in the repo or on a
game server; HTTPS only; logs never contain passwords or tickets; the game server checks `exp` and kicks banned sessions.

Host: AWS (decided). Start with one EC2 or Lightsail box running the game server, the auth service and Caddy (free
Let's Encrypt cert, needs a domain); move HTTPS to an ALB + ACM only when there is more than one instance.

Release: the full account system (all four stages, deployed) is what makes `v1.0.0`; until then it stays `0.x`.

Open: the final name rules and reserved-name list, duplicate-login behaviour (recommend: kick the old session).
