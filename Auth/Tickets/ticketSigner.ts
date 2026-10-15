import {
  createPrivateKey,
  createPublicKey,
  KeyObject,
  sign as cryptoSign,
  verify as cryptoVerify,
} from "node:crypto";
import { readFileSync } from "node:fs";

interface Claim {
  id: number;
  name: string;
  isAdmin: boolean;
}

export class TicketSigner {
  readonly #key: KeyObject;
  readonly #pub: KeyObject;

  // pem defaults to the file named by AUTH_KEY_PATH; tests pass their own.
  constructor(pem?: string) {
    if (pem === undefined) {
      const path = process.env.AUTH_KEY_PATH;
      if (!path) throw new Error("AUTH_KEY_PATH is not set");
      pem = readFileSync(path, "utf8");
    }
    this.#key = createPrivateKey(pem);
    this.#pub = createPublicKey(pem);
  }

  sign(claims: Claim, nowSeconds: number) {
    if (claims.name.length > 16) {
      throw new Error("Name length is too long (max 16)");
    }
    const exp: number = nowSeconds + 3600;
    const buf: Buffer = Buffer.alloc(10 + claims.name.length);
    // fill the buffer
    buf.writeUint32BE(claims.id, 0);
    buf.writeUint32BE(exp, 4);
    buf.writeUint8(claims.isAdmin ? 1 : 0, 8);
    buf.writeUint8(claims.name.length, 9);
    buf.write(claims.name, 10, "ascii");

    const sig = cryptoSign(null, buf, this.#key);
    return Buffer.concat([buf, sig]).toString("base64");
  }

  verify(ticket: string, nowSeconds: number): Claim | null {
    const buf = Buffer.from(ticket, "base64");
    const bufferLength = buf.length;
    if (buf.length < 10 + 64) return null;
    const nameLen = buf.readUint8(9);
    if (bufferLength !== 10 + nameLen + 64) return null;
    const payload = buf.subarray(0, buf.length - 64);
    const sig = buf.subarray(buf.length - 64);
    if (!cryptoVerify(null, payload, this.#pub, sig)) return null;
    const exp = payload.readUint32BE(4);
    if (exp <= nowSeconds) return null;

    return {
      id: payload.readUint32BE(0),
      isAdmin: payload.readUint8(8) === 1,
      name: payload.toString("ascii", 10),
    };
  }
}

/*
  ┌─────────┬─────────────────────────────────────────┐
  │  Bytes  │                  Field                  │
  ├─────────┼─────────────────────────────────────────┤
  │ 4       │ accountId (u32)                         │
  ├─────────┼─────────────────────────────────────────┤
  │ 4       │ exp (u32, unix seconds)                 │
  ├─────────┼─────────────────────────────────────────┤
  │ 1       │ isAdmin (0 or 1)                        │
  ├─────────┼─────────────────────────────────────────┤
  │ 1       │ nameLen (u8, at most 16)                │
  ├─────────┼─────────────────────────────────────────┤
  │ nameLen │ name (ASCII)                            │
  ├─────────┼─────────────────────────────────────────┤
  │ 64      │ Ed25519 signature over everything above │
  └─────────┴─────────────────────────────────────────┘
*/
