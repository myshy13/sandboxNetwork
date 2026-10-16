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
