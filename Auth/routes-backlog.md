# Auth service: routes backlog

Order matters: each item needs the ones above it. `[~]` = started, `[x]` = done and tested.
Bigger picture and decisions: `plan.md` at the repo root, "Accounts".

## Db (`Db/db.ts`), needed before the routes that use it

1. [x] `refresh_tokens` table (`tokenHash` unique, `accountId`, `exp`, `created`; many per account) and `Db` methods.
       Only a SHA-256 of the token is stored, so a leaked database can't be replayed.
   - [x] table, `createSession` (returns the plain token), daily cleanup of expired rows (stopped by `close()`).
   - [x] `getSession(token, now)`: hashes the token itself, never returns an expired row.
   - [x] `deleteSession(token)`, and (later) delete all of an account's sessions.
2. [x] `getById(id)`, for `/refresh` (it has an account id, not a name).
3. [x] `rotateSession(token, now, expires)`: one transaction, the delete's row count decides the winner.

## AuthRoutes (`Routes/authRoutes.ts`)

1. [x] `POST /register`
   - [x] Body schema (TypeBox), per-IP rate limit 5/hour, hash, `createAccount`, `409` when taken, `201` + ticket.
   - [x] Wired into `buildApp.ts`.
   - [x] Reserved names (contains `guest` or `admin`, case-insensitive): `400 { error: "Name not allowed" }`.
         `400`, not `409` (that means "taken") and not `406` (content negotiation).
   - [x] `inject` test: valid, same name in other case = `409`, short password = `400`, reserved name = `400`.
2. [x] `POST /login`: one `verify` per request (dummy inside it), `#isBanned`, ticket + refresh token, per-IP and
       per-name limits (`app.createRateLimit`, called by hand: a hook-based second limiter is skipped).
3. [x] `POST /refresh`
   - Hash the presented token, `getSession`, check `exp`.
   - Re-read the account (`getById`) so a ban takes effect, then reply with a new ticket.
   - Rotation (decided): every use replaces the refresh token with a new 30-day one; the old one stops working.
     Two requests with the same token must not both succeed.
4. [x] `POST /logout`: `deleteSession`; always reply `204`, even for an unknown token.

## AdminRoutes (`Routes/adminRoutes.ts`), for the admin panel

Every route here takes the caller's ticket in the `authorization` header and needs `isAdmin`.

1. [x] `POST /admin/ban`: `{ name, reason, hours }`, over `Db.setBan`; `204`, `404` unknown player. Tested
       (`Routes/adminRoutes.test.ts`).
   - [x] Decide what `hours: null` means (permanent?) and make the schema and handler agree; today the handler rejects it. **Decision:** permanent = omit `hours` (not `null`: Ajv coerces `0` to `null`)
   - [x] Upper limit on `hours` (schema `minimum`/`maximum`),\* and reject negative in the schema instead of the handler.
   - [x] Status codes: bad ticket `401` (now `400`), not an admin `403` (now `401`). Update tests and `docs.md`.
   - [x] Check the caller is still an admin and not banned in the db, not only in the ticket (up to 1 hour stale).
   - [x] One `onRequest` hook for the whole `/admin` scope (auth runs before body validation) so the next route can't skip it.
2. [x] `POST /admin/unban`: `{ name }`, over `Db.clearBan`; same auth as ban. Tested.
3. [~] Admin panel, in the game client (`Client/src/AdminPanel/`), over the routes above. Design:
   - [ ] List of online players, one row each: name, a `*guest*` tag, a **Ban** button. No `[Un]ban` toggle: the client
         can't know who is banned (no route returns it) and banned players are usually offline.
   - [ ] Guest rows (name starts with `"Guest "`, account names can't contain a space) and the admin's own row: Ban
         disabled. A guest has no account, so `/admin/ban` would answer `404`.
   - [ ] Ban opens a small form (reason 1 to 64, optional hours 1 to 8760) with Confirm and Cancel; never one click.
   - [ ] Separate Unban box: a `TextBox` for an account name and one Unban button.
   - [ ] Requests don't block the frame loop; per-row state (idle, form open, pending, failed + message), buttons off
         while pending. `204` closes the form, `401` refreshes the ticket and retries once, `403` hides the panel,
         `404` shows "no such player".
   - [ ] A toggle `Action` in `InputState` opens the panel and frees the mouse from mouselook.
   - [ ] Say in the UI that a ban takes effect at the player's next login: their ticket works for up to an hour and
         the game server isn't told (needs the stage-2 kick).
   - [ ] Show the panel only if the ticket says `isAdmin` (iOS/tvOS are guest-only, so never there).
   - [ ] Web build: the `authorization` header triggers a CORS preflight; try it against the real `CORS_ORIGIN`.
4. [ ] Maybe: `GET /admin/bans` (banned accounts, reasons, expiry) so the panel can list them and offer Unban per row.
5. [ ] Maybe: refuse `/admin/ban` when the target is an admin, so an admin can't lock out themselves or another admin.

## Scripts (not routes)

1. [x] `Admin/ban.ts`: `<name> "<reason>" [hours]`, over `Db.setBan`.
2. [x] `Admin/unban.ts`: `<name>`, over `Db.clearBan`.
3. [x] `Admin/promote.ts`: `<name> [--revoke]`, over `Db.setAdmin`. Not a route on purpose: the first admin comes from the box.

## Deliberately not routes (v1)

- **No `/verify`:** game servers check tickets locally with the public key.
- **Admin HTTP routes are limited to the admin panel's needs:** the CLI scripts stay for the operator.

## Later

Email + password reset, a revocation list endpoint for game servers.
