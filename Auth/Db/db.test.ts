import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { test, type TestContext } from "node:test";
import { Db } from "./db.js";

const now = 1_000_000;

// A fresh in-memory db with one account, so sessions have something to point at.
function setup(t: TestContext) {
  const db = new Db(":memory:");
  t.after(() => db.close());
  const id = db.createAccount("hamish", "not-a-real-hash");
  assert.ok(id !== null);
  return { db, id };
}

test("createSession returns a 43-character base64url token, different every time", (t) => {
  const { db, id } = setup(t);
  const a = db.createSession(id, now, now + 100);
  const b = db.createSession(id, now, now + 100);
  assert.match(a, /^[A-Za-z0-9_-]{43}$/);
  assert.notEqual(a, b);
});

test("getSession finds the row, and only the hash of the token is stored", (t) => {
  const { db, id } = setup(t);
  const token = db.createSession(id, now, now + 100);

  const row = db.getSession(token, now);
  assert.ok(row);
  assert.equal(row.accountId, id);
  assert.equal(row.exp, now + 100);
  assert.equal(row.created, now);
  assert.notEqual(row.tokenHash, token);
  assert.equal(row.tokenHash, createHash("sha256").update(token).digest("hex"));
});

test("an unknown token gives undefined and does not throw", (t) => {
  const { db, id } = setup(t);
  db.createSession(id, now, now + 100);
  for (const junk of ["", "abc", "x".repeat(43), "'; DROP TABLE refresh_tokens; --"])
    assert.equal(db.getSession(junk, now), undefined, junk);
});

test("an expired session is not returned, including in the very second it expires", (t) => {
  const { db, id } = setup(t);
  const token = db.createSession(id, now, now + 100);
  assert.ok(db.getSession(token, now + 99), "still valid one second before");
  assert.equal(db.getSession(token, now + 100), undefined, "expires at exp, same rule as tickets and bans");
  assert.equal(db.getSession(token, now + 101), undefined);
});

test("deleteSession removes one session and reports whether it did", (t) => {
  const { db, id } = setup(t);
  const a = db.createSession(id, now, now + 100);
  const b = db.createSession(id, now, now + 100);

  assert.equal(db.deleteSession(a), true);
  assert.equal(db.getSession(a, now), undefined);
  assert.equal(db.deleteSession(a), false, "already gone");
  assert.equal(db.deleteSession("never-existed"), false);
  assert.ok(db.getSession(b, now), "the account's other session is untouched");
});

test("a session cannot be created for an account that does not exist", (t) => {
  const { db } = setup(t);
  assert.throws(() => db.createSession(9999, now, now + 100));
});

test("getById finds an account, and gives undefined for an unknown id", (t) => {
  const { db, id } = setup(t);
  assert.equal(db.getById(id)?.name, "hamish");
  assert.equal(db.getById(id + 1), undefined);
});

test("rotateSession swaps the token: the old one dies, the new one works", (t) => {
  const { db, id } = setup(t);
  const old = db.createSession(id, now, now + 100);

  const rotated = db.rotateSession(old, now + 10, now + 500);
  assert.ok(rotated);
  assert.equal(rotated.accountId, id);
  assert.notEqual(rotated.token, old);
  assert.equal(db.getSession(old, now + 10), undefined);

  const row = db.getSession(rotated.token, now + 10);
  assert.ok(row);
  assert.equal(row.exp, now + 500);
  assert.equal(row.created, now + 10);
});

test("rotateSession gives nothing for an unknown, expired or already-rotated token", (t) => {
  const { db, id } = setup(t);
  const expired = db.createSession(id, now, now + 100);
  assert.equal(db.rotateSession("never-existed", now, now + 500), undefined);
  assert.equal(db.rotateSession(expired, now + 100, now + 500), undefined, "expires at exp");

  const live = db.createSession(id, now, now + 100);
  assert.ok(db.rotateSession(live, now, now + 500));
  assert.equal(db.rotateSession(live, now, now + 500), undefined, "a token can be rotated only once");
});

test("rotateSession leaves the account's other sessions alone", (t) => {
  const { db, id } = setup(t);
  const a = db.createSession(id, now, now + 100);
  const b = db.createSession(id, now, now + 100);
  db.rotateSession(a, now, now + 500);
  assert.ok(db.getSession(b, now));
});
