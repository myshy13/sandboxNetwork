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
const now = () => Math.floor(Date.now() / 1000);

// A fresh app and database per test, with an admin, a normal player and a victim to ban.
async function setup(t: TestContext) {
  const db = new Db(":memory:");
  const app = await buildApp(db, passwords, signer);
  t.after(() => app.close());

  const adminId = db.createAccount("boss", "unused-hash")!;
  const userId = db.createAccount("regular", "unused-hash")!;
  db.createAccount("victim", "unused-hash");
  db.setAdmin("boss", true);

  const adminTicket = signer.sign({ id: adminId, name: "boss", isAdmin: true }, now());
  const userTicket = signer.sign({ id: userId, name: "regular", isAdmin: false }, now());
  return { app, db, adminTicket, userTicket };
}

type App = Awaited<ReturnType<typeof setup>>["app"];

const ban = (app: App, payload: object, ticket?: string) =>
  app.inject({
    method: "POST",
    url: "/admin/ban",
    payload,
    headers: ticket === undefined ? {} : { authorization: ticket },
  });

const goodBody = { name: "victim", reason: "cheating", hours: 2 };

test("an admin ticket bans the player with the reason and expiry", async (t) => {
  const { app, db, adminTicket } = await setup(t);
  const before = now();
  const res = await ban(app, goodBody, adminTicket);
  assert.equal(res.statusCode, 204);

  const victim = db.getByName("victim")!;
  assert.equal(victim.banned, 1);
  assert.equal(victim.banReason, "cheating");
  // setBan stores now + hours * 3600, so allow a couple of seconds for the request itself
  assert.ok(victim.banExpires! >= before + 2 * 3600);
  assert.ok(victim.banExpires! <= now() + 2 * 3600);
});

test("a non-admin ticket is refused and nobody gets banned", async (t) => {
  const { app, db, userTicket } = await setup(t);
  const res = await ban(app, goodBody, userTicket);
  assert.equal(res.statusCode, 403);
  assert.equal(db.getByName("victim")!.banned, 0);
});

test("no authorization header is refused and nobody gets banned", async (t) => {
  const { app, db } = await setup(t);
  const res = await ban(app, goodBody);
  assert.equal(res.statusCode, 401);
  assert.equal(db.getByName("victim")!.banned, 0);
});

test("a garbage ticket is refused and nobody gets banned", async (t) => {
  const { app, db } = await setup(t);
  const res = await ban(app, goodBody, "not-a-ticket");
  assert.equal(res.statusCode, 401);
  assert.equal(db.getByName("victim")!.banned, 0);
});

test("an expired admin ticket is refused", async (t) => {
  const { app, db } = await setup(t);
  const stale = signer.sign({ id: 1, name: "boss", isAdmin: true }, now() - 2 * 3600);
  const res = await ban(app, goodBody, stale);
  assert.equal(res.statusCode, 401);
  assert.equal(db.getByName("victim")!.banned, 0);
});

test("a ticket signed by a different key is refused", async (t) => {
  const { app, db } = await setup(t);
  const otherPem = generateKeyPairSync("ed25519").privateKey.export({ type: "pkcs8", format: "pem" }).toString();
  const forged = new TicketSigner(otherPem).sign({ id: 1, name: "boss", isAdmin: true }, now());
  const res = await ban(app, goodBody, forged);
  assert.equal(res.statusCode, 401);
  assert.equal(db.getByName("victim")!.banned, 0);
});

test("banning an unknown player answers 404", async (t) => {
  const { app, adminTicket } = await setup(t);
  const res = await ban(app, { ...goodBody, name: "nobody" }, adminTicket);
  assert.equal(res.statusCode, 404);
});

test("bad hours are rejected with 400 and nobody gets banned", async (t) => {
  const { app, db, adminTicket } = await setup(t);
  for (const hours of [0, null, -1, 1.5, 8761]) {
    const res = await ban(app, { ...goodBody, hours }, adminTicket);
    assert.equal(res.statusCode, 400, `hours=${JSON.stringify(hours)}`);
  }
  assert.equal(db.getByName("victim")!.banned, 0);
});

test("a body missing a field is rejected with 400", async (t) => {
  const { app, db, adminTicket } = await setup(t);
  const res = await ban(app, { name: "victim", hours: 2 }, adminTicket);
  assert.equal(res.statusCode, 400);
  assert.equal(db.getByName("victim")!.banned, 0);
});

test("omitting hours bans permanently", async (t) => {
  const { app, db, adminTicket } = await setup(t);
  const res = await ban(app, { name: "victim", reason: "cheating" }, adminTicket);
  assert.equal(res.statusCode, 204);
  const victim = db.getByName("victim")!;
  assert.equal(victim.banned, 1);
  assert.equal(victim.banExpires, null);
});

test("an admin demoted since the ticket was issued is refused", async (t) => {
  const { app, db, adminTicket } = await setup(t);
  db.setAdmin("boss", false);
  const res = await ban(app, goodBody, adminTicket);
  assert.equal(res.statusCode, 403);
  assert.equal(db.getByName("victim")!.banned, 0);
});

test("a banned admin is refused", async (t) => {
  const { app, db, adminTicket } = await setup(t);
  db.setBan("boss", "oops", null);
  const res = await ban(app, goodBody, adminTicket);
  assert.equal(res.statusCode, 403);
  assert.equal(db.getByName("victim")!.banned, 0);
});

test("auth runs before the body check, so a bad body without a ticket is 401", async (t) => {
  const { app } = await setup(t);
  const res = await ban(app, { nonsense: true });
  assert.equal(res.statusCode, 401);
});

// ==== unban ==== //

const unban = (app: App, payload: object, ticket?: string) =>
  app.inject({
    method: "POST",
    url: "/admin/unban",
    payload,
    headers: ticket === undefined ? {} : { authorization: ticket },
  });

test("an admin unbans a banned player and the ban fields are cleared", async (t) => {
  const { app, db, adminTicket } = await setup(t);
  db.setBan("victim", "cheating", now() + 3600);

  const res = await unban(app, { name: "victim" }, adminTicket);
  assert.equal(res.statusCode, 204);

  const victim = db.getByName("victim")!;
  assert.equal(victim.banned, 0);
  assert.equal(victim.banReason, null);
  assert.equal(victim.banExpires, null);
});

test("unbanning a player who isn't banned is still 204", async (t) => {
  const { app, adminTicket } = await setup(t);
  const res = await unban(app, { name: "victim" }, adminTicket);
  assert.equal(res.statusCode, 204);
});

test("unbanning an unknown player answers 404", async (t) => {
  const { app, adminTicket } = await setup(t);
  const res = await unban(app, { name: "nobody" }, adminTicket);
  assert.equal(res.statusCode, 404);
});

test("a non-admin can't unban", async (t) => {
  const { app, db, userTicket } = await setup(t);
  db.setBan("victim", "cheating", null);
  const res = await unban(app, { name: "victim" }, userTicket);
  assert.equal(res.statusCode, 403);
  assert.equal(db.getByName("victim")!.banned, 1);
});

test("no ticket and a garbage ticket can't unban", async (t) => {
  const { app, db } = await setup(t);
  db.setBan("victim", "cheating", null);
  assert.equal((await unban(app, { name: "victim" })).statusCode, 401);
  assert.equal((await unban(app, { name: "victim" }, "not-a-ticket")).statusCode, 401);
  assert.equal(db.getByName("victim")!.banned, 1);
});

test("a banned admin can't unban themselves", async (t) => {
  const { app, db, adminTicket } = await setup(t);
  db.setBan("boss", "oops", null);
  const res = await unban(app, { name: "boss" }, adminTicket);
  assert.equal(res.statusCode, 403);
  assert.equal(db.getByName("boss")!.banned, 1);
});

test("an unban body missing the name or with extra fields is rejected with 400", async (t) => {
  const { app, adminTicket } = await setup(t);
  assert.equal((await unban(app, {}, adminTicket)).statusCode, 400);
  assert.equal((await unban(app, { name: "" }, adminTicket)).statusCode, 400);
});
