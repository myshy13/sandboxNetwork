import { generateKeyPairSync } from "node:crypto";
import { writeFileSync } from "node:fs";

// Usage: tsx Tickets/keygen.ts <private.pem> <public.key>
const [privPath, pubPath] = process.argv.slice(2);
if (!privPath || !pubPath) {
  console.error("usage: keygen <private.pem> <public.key>");
  process.exit(1);
}

const { publicKey, privateKey } = generateKeyPairSync("ed25519");

// "wx" refuses to overwrite: a new key would invalidate every game server's copy.
writeFileSync(privPath, privateKey.export({ type: "pkcs8", format: "pem" }), { flag: "wx", mode: 0o600 });

// SPKI DER is a 12-byte header then the 32 raw key bytes the game server wants.
writeFileSync(pubPath, publicKey.export({ type: "spki", format: "der" }).subarray(-32), { flag: "wx" });

console.log(`wrote ${privPath} (keep secret) and ${pubPath} (give to game servers)`);
