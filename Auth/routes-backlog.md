# Auth service: routes backlog

Order matters: each item needs the ones above it. `[~]` = started, `[x]` = done and tested.
Bigger picture and decisions: `plan.md` at the repo root, "Accounts".

## Db (`Db/db.ts`), needed before the routes that use it

1. [ ] `sessions` table: `tokenHash`, `accountId`, `exp`; `createSession`, `getSession`, `deleteSession`.
       Store only a SHA-256 of the random token, so a leaked database can't be replayed.
2. [ ] `getById(id)`, for `/refresh` (it has an account id, not a name).

## AuthRoutes (`Routes/authRoutes.ts`)

1. [~] `POST /register`
   - [x] Body schema (TypeBox), per-IP rate limit 5/hour, hash, `createAccount`, `409` when taken, `201` + ticket.
   - [ ] Wire `new AuthRoutes(...).register(app)` into `buildApp.ts`.
   - [ ] Reserved names (contains `guest` or `admin`, case-insensitive): `400 { error: "Name not allowed" }`.
         `400`, not `409` (that means "taken") and not `406` (content negotiation).
   - [ ] `inject` test: valid, same name in other case = `409`, short password = `400`, reserved name = `400`.
2. [ ] `POST /login`
   - Verify with the dummy hash when the name is unknown; same error for unknown name and wrong password.
   - Reject `banned` accounts (honour `banExpires`).
   - Reply with a ticket and a refresh token (`createSession`).
   - Rate limit 10/minute per IP, plus per name (`keyGenerator`, `hook: "preHandler"` so the body is parsed).
3. [ ] `POST /refresh`
   - Hash the presented token, `getSession`, check `exp`.
   - Re-read the account (`getById`) so a ban takes effect, then reply with a new ticket.
   - Decide: rotate the refresh token on every use, or keep it until expiry.
4. [ ] `POST /logout`: `deleteSession`; always reply `204`, even for an unknown token.

## Scripts (not routes)

1. [ ] `Admin/ban.ts`: `<name> "<reason>" [hours]`, over `Db.setBan`.
2. [ ] `Admin/unban.ts`: `<name>`, over `Db.clearBan`.

## Deliberately not routes (v1)

- **No `/verify`:** game servers check tickets locally with the public key.
- **No admin HTTP routes:** the CLI scripts do bans until there's a real need.

## Later

Email + password reset, a revocation list endpoint for game servers, an admin web UI.
