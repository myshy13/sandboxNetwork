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

## Scripts (not routes)

1. [ ] `Admin/ban.ts`: `<name> "<reason>" [hours]`, over `Db.setBan`.
2. [ ] `Admin/unban.ts`: `<name>`, over `Db.clearBan`.

## Deliberately not routes (v1)

- **No `/verify`:** game servers check tickets locally with the public key.
- **No admin HTTP routes:** the CLI scripts do bans until there's a real need.

## Later

Email + password reset, a revocation list endpoint for game servers, an admin web UI.
