import assert from "node:assert/strict";
import { generateKeyPairSync } from "node:crypto";
import { test, type TestContext } from "node:test";
import buildApp from "../buildApp.js";
import { Db } from "../Db/db.js";
import { Passwords } from "../Passwords/passwords.js";
import { TicketSigner } from "../Tickets/ticketSigner.js";

const pem = generateKeyPairSync("ed25519").privateKey.export({ type: "pkcs8", format: "pem" }).toString();
const signer = new TicketSigner(pem);
const passwords = new Passwords();
const goodPassword = "correct-horse";

// A fresh app and database per test, so the rate limiter starts empty each time.
async function setup(t: TestContext) {
  const db = new Db(":memory:");
  const app = await buildApp(db, passwords, signer);
  t.after(() => app.close());
  return { app, db };
}

type App = Awaited<ReturnType<typeof setup>>["app"];

const register = (app: App, payload: object) => app.inject({ method: "POST", url: "/register", payload });

test("valid registration returns 201 and a ticket for that account", async (t) => {
  const { app } = await setup(t);
  const res = await register(app, { name: "hamish", password: goodPassword });
  assert.equal(res.statusCode, 201);

  const claims = signer.verify(res.json().ticket, Math.floor(Date.now() / 1000));
  assert.ok(claims, "ticket should verify");
  assert.equal(claims.name, "hamish");
  assert.equal(claims.isAdmin, false);
});

test("the stored password is an Argon2 hash that verifies", async (t) => {
  const { app, db } = await setup(t);
  await register(app, { name: "hamish", password: goodPassword });

  const account = db.getByName("hamish");
  assert.ok(account);
  assert.notEqual(account.hash, goodPassword);
  assert.equal(await passwords.verify(account.hash, goodPassword), true);
  assert.equal(await passwords.verify(account.hash, "wrong-password"), false);
  assert.equal(account.isAdmin, 0);
  assert.equal(account.banned, 0);
});

test("the same name in another letter case is 409", async (t) => {
  const { app } = await setup(t);
  assert.equal((await register(app, { name: "Hamish", password: goodPassword })).statusCode, 201);
  assert.equal((await register(app, { name: "hAMISH", password: goodPassword })).statusCode, 409);
});

test("passwords outside 8..128 characters are 400", async (t) => {
  const { app } = await setup(t);
  assert.equal((await register(app, { name: "hamish", password: "a".repeat(7) })).statusCode, 400);
  assert.equal((await register(app, { name: "hamish", password: "a".repeat(129) })).statusCode, 400);
});

test("names that break the pattern are 400", async (t) => {
  // One app per name: every request counts towards the 5/hour limit, even a rejected one.
  for (const name of ["ab", "a".repeat(17), "bad name", "bad-name", "naïve", ""]) {
    const { app } = await setup(t);
    assert.equal((await register(app, { name, password: goodPassword })).statusCode, 400, `name: ${name}`);
  }
});

test("reserved names are 400 and no account is created", async (t) => {
  const { app, db } = await setup(t);
  for (const name of ["Guest_1", "xAdMiNx"]) {
    const res = await register(app, { name, password: goodPassword });
    assert.equal(res.statusCode, 400, `name: ${name}`);
    assert.equal(db.getByName(name), undefined, `${name} must not exist`);
  }
});

// Fastify's default Ajv has removeAdditional on, so an unknown field is dropped, not rejected (status stays 201).
test("isAdmin in the body cannot make an admin", async (t) => {
  const { app, db } = await setup(t);
  const res = await register(app, { name: "hamish", password: goodPassword, isAdmin: true });

  const claims = signer.verify(res.json().ticket, Math.floor(Date.now() / 1000));
  assert.ok(claims);
  assert.equal(claims.isAdmin, false);
  assert.equal(db.getByName("hamish")?.isAdmin, 0);
});

test("missing fields are 400", async (t) => {
  const { app } = await setup(t);
  assert.equal((await register(app, { name: "hamish" })).statusCode, 400);
  assert.equal((await register(app, { password: goodPassword })).statusCode, 400);
});

test("the sixth registration from one IP in an hour is 429", async (t) => {
  const { app } = await setup(t);
  for (let i = 0; i < 5; i++)
    assert.equal((await register(app, { name: `user_${i}`, password: goodPassword })).statusCode, 201);
  assert.equal((await register(app, { name: "user_5", password: goodPassword })).statusCode, 429);
});

// ==== login ==== //

const now = () => Math.floor(Date.now() / 1000);

// remoteAddress lets a test come from a chosen IP, so the per-IP and per-name limits can be told apart.
const login = (app: App, payload: object, remoteAddress?: string) =>
  app.inject({ method: "POST", url: "/login", payload, remoteAddress });

// Inserts an account straight into the db (no HTTP, so no register rate limit).
const makeAccount = async (db: Db, name: string) => db.createAccount(name, await passwords.hash(goodPassword));

test("login with the right password returns 200 and a ticket for that account", async (t) => {
  const { app, db } = await setup(t);
  const id = await makeAccount(db, "hamish");

  const res = await login(app, { name: "hamish", password: goodPassword });
  assert.equal(res.statusCode, 200);

  const claims = signer.verify(res.json().ticket, now());
  assert.deepEqual(claims, { id, name: "hamish", isAdmin: false });
});

test("login also works in another letter case of the name", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "Hamish");
  assert.equal((await login(app, { name: "hAMISH", password: goodPassword })).statusCode, 200);
});

test("login returns a refresh token: 43 base64url characters", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "hamish");
  const res = await login(app, { name: "hamish", password: goodPassword });
  assert.match(res.json().refreshToken, /^[A-Za-z0-9_-]{43}$/);
});

test("logging in twice gives two different refresh tokens (many sessions per account)", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "hamish");
  const first  = await login(app, { name: "hamish", password: goodPassword });
  const second = await login(app, { name: "hamish", password: goodPassword });
  assert.equal(second.statusCode, 200);
  assert.notEqual(first.json().refreshToken, second.json().refreshToken);
});

test("a wrong password and an unknown name get the same 401", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "hamish");

  const wrong   = await login(app, { name: "hamish", password: "wrong-password" });
  const unknown = await login(app, { name: "nobody", password: goodPassword });
  assert.equal(wrong.statusCode, 401);
  assert.equal(unknown.statusCode, 401);
  assert.deepEqual(wrong.json(), unknown.json());
});

test("a permanent ban is 403 with the reason, and stays in place", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "hamish");
  db.setBan("hamish", "cheating", null);

  const res = await login(app, { name: "hamish", password: goodPassword });
  assert.equal(res.statusCode, 403);
  assert.deepEqual(res.json(), { error: "Account banned", reason: "cheating", expires: null });

  assert.equal(db.getByName("hamish")?.banned, 1, "logging in must not lift the ban");
  assert.equal((await login(app, { name: "hamish", password: goodPassword })).statusCode, 403);
});

test("a ban that ends in the future is 403 with its expiry", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "hamish");
  const expires = now() + 3600;
  db.setBan("hamish", "spam", expires);

  const res = await login(app, { name: "hamish", password: goodPassword });
  assert.equal(res.statusCode, 403);
  assert.deepEqual(res.json(), { error: "Account banned", reason: "spam", expires });
});

test("an expired ban lets the player in", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "hamish");
  db.setBan("hamish", "spam", now() - 10);

  const res = await login(app, { name: "hamish", password: goodPassword });
  assert.equal(res.statusCode, 200);
  assert.ok(signer.verify(res.json().ticket, now()));
});

test("a ban that ends in this very second is over", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "hamish");
  db.setBan("hamish", "spam", now());

  // Check the ticket too: a handler that sends nothing still answers 200 with an empty body.
  const res = await login(app, { name: "hamish", password: goodPassword });
  assert.equal(res.statusCode, 200);
  assert.ok(signer.verify(res.json().ticket, now()));
});

test("a banned account with the wrong password gets 401, so the ban is not revealed", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "hamish");
  db.setBan("hamish", "cheating", null);

  const banned  = await login(app, { name: "hamish", password: "wrong-password" });
  const unknown = await login(app, { name: "nobody", password: "wrong-password" });
  assert.equal(banned.statusCode, 401);
  assert.deepEqual(banned.json(), unknown.json());
});

test("a body that fails the schema is 400", async (t) => {
  const { app } = await setup(t);
  for (const payload of [{ name: "hamish" }, { password: goodPassword }, { name: "hamish", password: "" }, { name: "", password: goodPassword }, { name: "a".repeat(17), password: goodPassword }])
    assert.equal((await login(app, payload)).statusCode, 400, JSON.stringify(payload));
});

test("the 11th login from one IP in a minute is 429", async (t) => {
  const { app } = await setup(t);
  // A different name each time, so only the per-IP limit can trip.
  for (let i = 0; i < 10; i++)
    assert.equal((await login(app, { name: `user_${i}`, password: goodPassword })).statusCode, 401);
  assert.equal((await login(app, { name: "user_10", password: goodPassword })).statusCode, 429);
});

test("the 11th login for one name is 429 even from different IPs and letter cases", async (t) => {
  const { app, db } = await setup(t);
  await makeAccount(db, "hamish");
  const spellings = ["hamish", "Hamish", "HAMISH", "hAmish", "haMish"];

  // Every attempt comes from its own IP, so only the per-name limit can trip.
  for (let i = 0; i < 10; i++)
    assert.equal((await login(app, { name: spellings[i % 5]!, password: "wrong-password" }, `10.0.0.${i}`)).statusCode, 401);
  assert.equal((await login(app, { name: "HAMISH", password: goodPassword }, "10.0.0.99")).statusCode, 429);

  // Another account from yet another IP is not affected.
  await makeAccount(db, "someone");
  assert.equal((await login(app, { name: "someone", password: goodPassword }, "10.0.0.100")).statusCode, 200);
});

// Timing smoke test: catches a gross leak (e.g. one extra Argon2 run), not a subtle one.
const median = (xs: number[]) => [...xs].sort((a, b) => a - b)[Math.floor(xs.length / 2)]!;

test("an unknown name takes about as long as a wrong password", { timeout: 60_000 }, async (t) => {
  const { app, db } = await setup(t);
  const warmup  = 2;
  const samples = 15;

  // One shared hash: hashing 17 times would only slow the test down.
  const hash = await passwords.hash(goodPassword);
  for (let i = 0; i < warmup + samples; i++) db.createAccount(`user_${i}`, hash);

  const wrong: number[]   = [];
  const unknown: number[] = [];
  for (let i = 0; i < warmup + samples; i++) {
    // A fresh IP and name each time, so neither rate limit gets involved.
    let start = performance.now();
    await login(app, { name: `user_${i}`, password: "wrong-password" }, `10.1.0.${i}`);
    const wrongMs = performance.now() - start;

    start = performance.now();
    await login(app, { name: `nobody_${i}`, password: "wrong-password" }, `10.2.0.${i}`);
    const unknownMs = performance.now() - start;

    if (i >= warmup) {
      wrong.push(wrongMs);
      unknown.push(unknownMs);
    }
  }

  const w = median(wrong);
  const u = median(unknown);
  assert.ok(
    Math.abs(w - u) / Math.max(w, u) < 0.3,
    `wrong password ${w.toFixed(1)} ms vs unknown name ${u.toFixed(1)} ms (median of ${samples})`,
  );
});

// ==== refresh ==== //

const refresh = (app: App, refreshToken: string, remoteAddress?: string) =>
  app.inject({ method: "POST", url: "/refresh", payload: { refreshToken }, remoteAddress });

// An account plus a live session, straight in the db (no Argon2, no HTTP).
function withSession(db: Db, name = "hamish") {
  const id = db.createAccount(name, "not-a-real-hash");
  assert.ok(id !== null);
  return { id, token: db.createSession(id, now(), now() + 1000) };
}

test("refresh returns 200, a ticket for that account and a different refresh token", async (t) => {
  const { app, db } = await setup(t);
  const { id, token } = withSession(db);

  const res = await refresh(app, token);
  assert.equal(res.statusCode, 200);
  assert.deepEqual(signer.verify(res.json().ticket, now()), { id, name: "hamish", isAdmin: false });
  assert.match(res.json().refreshToken, /^[A-Za-z0-9_-]{43}$/);
  assert.notEqual(res.json().refreshToken, token);
});

test("the old refresh token stops working and the new one can be rotated again", async (t) => {
  const { app, db } = await setup(t);
  const { token } = withSession(db);

  const first = await refresh(app, token);
  assert.equal((await refresh(app, token)).statusCode, 401, "the used token is dead");

  const second = await refresh(app, first.json().refreshToken);
  assert.equal(second.statusCode, 200);
  assert.equal((await refresh(app, second.json().refreshToken)).statusCode, 200);
});

test("two simultaneous refreshes with one token: exactly one wins", async (t) => {
  const { app, db } = await setup(t);
  const { token } = withSession(db);

  const codes = (await Promise.all([refresh(app, token), refresh(app, token), refresh(app, token)])).map(
    (r) => r.statusCode,
  );
  assert.deepEqual(codes.sort(), [200, 401, 401]);
});

test("unknown, expired and used tokens all get the same 401", async (t) => {
  const { app, db } = await setup(t);
  const { id, token: used } = withSession(db);
  await refresh(app, used);
  const expired = db.createSession(id, now() - 200, now() - 100);

  const answers = await Promise.all(
    [used, expired, "A".repeat(43)].map(async (tok) => {
      const res = await refresh(app, tok);
      return { status: res.statusCode, body: res.json() };
    }),
  );
  assert.deepEqual(answers[0], { status: 401, body: { error: "Invalid refresh token" } });
  assert.deepEqual(answers[1], answers[0]);
  assert.deepEqual(answers[2], answers[0]);
});

test("a body that is not a 43-character base64url token is 400", async (t) => {
  const { app } = await setup(t);
  for (const token of ["", "a".repeat(42), "a".repeat(44), "a".repeat(42) + "+", "a".repeat(42) + " "])
    assert.equal((await refresh(app, token)).statusCode, 400, JSON.stringify(token));
  assert.equal((await app.inject({ method: "POST", url: "/refresh", payload: {} })).statusCode, 400);
});

test("a banned account gets 403 with the reason, and its token is not used up", async (t) => {
  const { app, db } = await setup(t);
  const { token } = withSession(db);
  db.setBan("hamish", "cheating", null);

  const res = await refresh(app, token);
  assert.equal(res.statusCode, 403);
  assert.deepEqual(res.json(), { error: "Account banned", reason: "cheating", expires: null });

  db.clearBan("hamish");
  assert.equal((await refresh(app, token)).statusCode, 200, "the same token still works after the ban ends");
});

test("an expired ban does not block a refresh", async (t) => {
  const { app, db } = await setup(t);
  const { token } = withSession(db);
  db.setBan("hamish", "spam", now() - 10);
  assert.equal((await refresh(app, token)).statusCode, 200);
});

test("the 31st refresh from one IP in a minute is 429", async (t) => {
  const { app } = await setup(t);
  for (let i = 0; i < 30; i++) assert.equal((await refresh(app, "A".repeat(43))).statusCode, 401);
  assert.equal((await refresh(app, "A".repeat(43))).statusCode, 429);
});

// ==== logout ====
// Written before the route, from its contract: always 204 with no body, removes only that one session.

const logout = (app: App, refreshToken: string, remoteAddress?: string) =>
  app.inject({ method: "POST", url: "/logout", payload: { refreshToken }, remoteAddress });

test("logout answers 204 with no body, and the token stops working", async (t) => {
  const { app, db } = await setup(t);
  const { token } = withSession(db);

  const res = await logout(app, token);
  assert.equal(res.statusCode, 204);
  assert.equal(res.body, "");
  assert.equal((await refresh(app, token)).statusCode, 401, "a logged-out token cannot refresh");
});

test("logout answers the same 204 for a token that was never valid, and for one used twice", async (t) => {
  const { app, db } = await setup(t);
  const { token } = withSession(db);
  await logout(app, token);

  for (const tok of [token, "A".repeat(43)]) {
    const res = await logout(app, tok);
    assert.equal(res.statusCode, 204, "must not reveal whether the token existed");
    assert.equal(res.body, "");
  }
});

test("logout removes only that session, not the account's others", async (t) => {
  const { app, db } = await setup(t);
  const { id, token: phone } = withSession(db);
  const pc = db.createSession(id, now(), now() + 1000);

  await logout(app, phone);
  assert.equal((await refresh(app, phone)).statusCode, 401);
  assert.equal((await refresh(app, pc)).statusCode, 200, "the other device stays logged in");
});

test("logout works for a banned account too", async (t) => {
  const { app, db } = await setup(t);
  const { token } = withSession(db);
  db.setBan("hamish", "cheating", null);

  assert.equal((await logout(app, token)).statusCode, 204);
  db.clearBan("hamish");
  assert.equal((await refresh(app, token)).statusCode, 401, "the session really was deleted");
});

test("logout needs no ticket or password, only the token", async (t) => {
  const { app, db } = await setup(t);
  const { token } = withSession(db);
  const res = await app.inject({ method: "POST", url: "/logout", payload: { refreshToken: token } });
  assert.equal(res.statusCode, 204);
});

test("a body that is not a 43-character base64url token is 400", async (t) => {
  const { app } = await setup(t);
  for (const token of ["", "a".repeat(42), "a".repeat(44), "a".repeat(42) + "+"])
    assert.equal((await logout(app, token)).statusCode, 400, JSON.stringify(token));
  assert.equal((await app.inject({ method: "POST", url: "/logout", payload: {} })).statusCode, 400);
});

test("logout is rate limited per IP", async (t) => {
  const { app } = await setup(t);
  // The exact number is the route's choice; it must kick in well before a thousand requests.
  let limited = false;
  for (let i = 0; i < 1000 && !limited; i++)
    limited = (await logout(app, "A".repeat(43))).statusCode === 429;
  assert.ok(limited, "no 429 after 1000 logouts from one IP");
});
