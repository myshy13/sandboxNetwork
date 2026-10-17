import * as argon2 from "argon2";
import { randomUUID } from "node:crypto";

export class Passwords {
  readonly #dummy: Promise<string> = argon2.hash(randomUUID());

  hash(password: string): Promise<string> {
    return argon2.hash(password);
  }

  async verify(hash: string | undefined, password: string): Promise<boolean> {
    if (hash === undefined) {
      await argon2.verify(await this.#dummy, password);
      return false;
    }
    return await argon2.verify(hash, password);
  }
}
