# Auth service: route reference

One entry per route, added when the route is created (rule in `CLAUDE.md`). Bodies are JSON
(`content-type: application/json`). Every error is `{ "error": string }`. Unknown body fields are dropped, not
rejected (Fastify's default Ajv). Route order and status: `routes-backlog.md`. Ticket bytes: top of
`Tickets/ticketSigner.ts`.

Statuses every route can return without its handler running: `400` (body fails the schema), `429` (rate limit),
`413` (body over 1024 bytes).

## Browser CORS

Set `CORS_ORIGIN` to the exact origin serving the web client (scheme, host and
port, with no path). The Auth service allows browser requests from that origin
only. Leave it unset for native-only local development.

## POST /register

Creates an account and logs it in. Used by the client's Register screen.

- **Rate limit:** 5 per hour per IP.
- **Body** (`#registerBody`):

  | Field      | Type   | Rules                                                                                                               |
  | ---------- | ------ | ------------------------------------------------------------------------------------------------------------------- |
  | `name`     | string | `^[A-Za-z0-9_]{3,16}$`, and must not contain a reserved word (`guest`, `admin`, `staff`, `owner`), case-insensitive |
  | `password` | string | 8 to 128 characters                                                                                                 |

- **Responses:**

  | Status | Body                              | When                                     |
  | ------ | --------------------------------- | ---------------------------------------- |
  | `201`  | `{ ticket }`                      | account created                          |
  | `400`  | `{ error: "Name not allowed" }`   | reserved word in the name                |
  | `409`  | `{ error: "Name already taken" }` | name exists (names are case-insensitive) |

- `ticket` is base64 and lasts 1 hour. The account is never an admin from this route.

## POST /login

Exchanges a name and password for a ticket and a refresh token. Used by the client's Login screen.

- **Rate limit:** two, both answering `429`: 10 per minute per IP, and 10 per 15 minutes per account name
  (case-insensitive; every attempt counts, so anyone can use up a victim's allowance).
- **Body** (`#loginBody`), deliberately looser than register so rule changes can't lock out old accounts:

  | Field      | Type   | Rules                          |
  | ---------- | ------ | ------------------------------ |
  | `name`     | string | 1 to 16 characters, no pattern |
  | `password` | string | 1 to 128 characters            |

- **Responses (agreed contract):**

  | Status | Body                                           | When                                                                         |
  | ------ | ---------------------------------------------- | ---------------------------------------------------------------------------- |
  | `200`  | `{ ticket, refreshToken }`                     | name and password correct, not banned                                        |
  | `401`  | `{ error: "Invalid username or password" }`    | unknown name **or** wrong password (identical)                               |
  | `403`  | `{ error: "Account banned", reason, expires }` | correct password but banned; `expires` is unix seconds or `null` (permanent) |

- `refreshToken` is 43 base64url characters (32 random bytes), valid 30 days, and an account may hold many at once.
  The per-name limit answers `429` with `{ error: "Too many attempts" }` and a `retry-after` header.
- The ban is checked only after the password is correct. `refreshToken` is returned in plain text only here; the
  server stores a hash.

## POST /refresh

Trades a refresh token for a new ticket and a **new** refresh token (rotation). Used by the client a minute before
its ticket expires, and on launch if it has a stored token.

- **Rate limit:** 30 per minute per IP.
- **Body** (`#refreshBody`):

  | Field          | Type   | Rules                                   |
  | -------------- | ------ | --------------------------------------- |
  | `refreshToken` | string | exactly 43 base64url characters (`^[A-Za-z0-9_-]{43}$`) |

- **Responses:**

  | Status | Body                                           | When                                                                   |
  | ------ | ---------------------------------------------- | ---------------------------------------------------------------------- |
  | `200`  | `{ ticket, refreshToken }`                     | token valid and the account is not banned                              |
  | `401`  | `{ error: "Invalid refresh token" }`           | unknown, expired **or already used** (identical for all three)         |
  | `403`  | `{ error: "Account banned", reason, expires }` | valid token but the account is banned (only a token made after the ban; see below) |

- The old token is deleted when the new one is issued, so each token works once; the new one lasts 30 days from now.
  Two simultaneous requests with the same token: exactly one gets `200`.
- Ticket claims (`id`, `name`, `isAdmin`) come from the account row, not from the client.

## POST /logout

Ends one login session by deleting its refresh token. Used by the client's Sign out button.

- **Rate limit:** 10 per minute per IP.
- **Body** (`#logoutBody`): `{ refreshToken }`, exactly 43 base64url characters (`^[A-Za-z0-9_-]{43}$`).
- **Responses:**

  | Status | Body | When                                                                                  |
  | ------ | ---- | ------------------------------------------------------------------------------------- |
  | `204`  | none | always: a live token, an unknown one, or one already logged out (identical on purpose) |

- It removes only that one session; the account's other devices stay logged in.
- No ticket or password is needed, only the token.
- A ticket already issued keeps working until its `exp` (up to 1 hour). Logging out cannot recall it; the client
  should just discard it.

## POST /admin/ban

Bans an account by name, for a number of hours. Admin only.

- **Auth:** the caller's ticket (from `/login` or `/refresh`) in the `authorization` header, as the bare ticket with
  no `Bearer ` prefix. It must verify and have `isAdmin`; the caller's identity comes from it, never from the body.
- **Rate limit:** the global 100 per minute per IP.
- **Body** (`#banPlayerBody`):

  | Field    | Type    | Rules                                                                          |
  | -------- | ------- | ------------------------------------------------------------------------------ |
  | `name`   | string  | 1 to 16 characters, the account to ban                                         |
  | `reason` | string  | 1 to 64 characters                                                             |
  | `hours`  | integer | how long the ban lasts; it expires at now + `hours` * 3600 (unix seconds)      |

- **Responses:**

  | Status | Body                            | When                                                |
  | ------ | ------------------------------- | --------------------------------------------------- |
  | `204`  | none                            | the ban was set                                     |
  | `400`  | `{ error: "Or in the past" }`   | `hours` is `null`, `0` or negative                  |
  | `400`  | `{ error: "Ticket invalid" }`   | ticket malformed, bad signature or expired          |
  | `401`  | `{ error: "Not an admin" }`     | valid ticket, but `isAdmin` is false                |
  | `404`  | `{ error: "Player not found" }` | no account with that name                           |

- Same effect as `Admin/ban.ts` (`Db.setBan`): all of the account's refresh tokens are deleted. A ticket already
  issued still works until its `exp`.
- `isAdmin` is read from the ticket, not the db, so a demoted admin keeps access until their ticket expires.

## Bans (`Admin/ban.ts`, `Admin/unban.ts`)

Run from `Auth/` (so dotenv finds `.env`): `pnpm tsx Admin/ban.ts <name> <reason> [hours]` and
`pnpm tsx Admin/unban.ts <name>`. No hours means permanent. Both open the same db file as the server (`Db/dbPath.ts`:
`AUTH_DB_PATH`, else `Auth/app.db`).

- Banning sets the ban and deletes **all** of that account's refresh tokens in one transaction (`Db.setBan`), so every
  device is signed out at once and `/refresh` answers `401`. Unbanning does not restore them; the player logs in again.
- Login still answers `403` with the reason for a banned account that gives the right password.
- A ticket already issued keeps working until its `exp` (up to 1 hour); a ban cannot recall it. Stage 2: the game server
  should kick banned sessions (a ban list pushed to it).
