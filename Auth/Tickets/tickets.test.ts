import { test } from "node:test";
import assert from "node:assert/strict";
import { generateKeyPairSync } from "node:crypto";
import { TicketSigner } from "./ticketSigner.js";

const newPem = () =>
  generateKeyPairSync("ed25519").privateKey.export({ type: "pkcs8", format: "pem" }).toString();

const now    = 1_000_000;
const claim  = { id: 7, name: "hamish", isAdmin: false };
const signer = new TicketSigner(newPem());

// Flip one bit of the byte at index (negative = from the end), return as base64.
const flip = (ticket: string, index: number): string => {
  const buf = Buffer.from(ticket, "base64");
  const at = index < 0 ? buf.length + index : index;
  buf[at] = buf[at]! ^ 1;
  return buf.toString("base64");
};

test("round trip returns the claims", () => {
  assert.deepEqual(signer.verify(signer.sign(claim, now), now), claim);
});

test("admin flag survives", () => {
  const admin = { ...claim, isAdmin: true };
  assert.deepEqual(signer.verify(signer.sign(admin, now), now), admin);
});

test("a changed payload byte is rejected", () => {
  assert.equal(signer.verify(flip(signer.sign(claim, now), 4), now), null);
});

test("a changed signature byte is rejected", () => {
  assert.equal(signer.verify(flip(signer.sign(claim, now), -1), now), null);
});

test("expired ticket is rejected", () => {
  const ticket = signer.sign(claim, now);
  assert.notEqual(signer.verify(ticket, now + 3599), null);
  assert.equal(signer.verify(ticket, now + 3600), null);
});

test("a ticket signed by another key is rejected", () => {
  const other = new TicketSigner(newPem());
  assert.equal(signer.verify(other.sign(claim, now), now), null);
});

test("garbage input returns null and does not throw", () => {
  for (const junk of ["", "abc", "!!!!", Buffer.alloc(73).toString("base64"), Buffer.alloc(200).toString("base64")])
    assert.equal(signer.verify(junk, now), null);
});

test("name longer than 16 is refused", () => {
  assert.throws(() => signer.sign({ ...claim, name: "a".repeat(17) }, now));
});
