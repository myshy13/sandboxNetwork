# Auth service: route reference

One entry per route, added when the route is created (rule in `CLAUDE.md`). Bodies are JSON
(`content-type: application/json`). Every error is `{ "error": string }`. Unknown body fields are dropped, not
rejected (Fastify's default Ajv). Route order and status: `routes-backlog.md`. Ticket bytes: top of
`Tickets/ticketSigner.ts`.

Statuses every route can return without its handler running: `400` (body fails the schema), `429` (rate limit),
`413` (body over 1024 bytes).

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
  | `403`  | `{ error: "Account banned", reason, expires }` | valid token but the account is banned; the token is **not** used up    |

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
