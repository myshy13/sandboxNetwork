import Database from "better-sqlite3";
import { createHash, randomBytes } from "node:crypto";

// One row of the accounts table; booleans are 0/1 because SQLite has no bool.
export interface Account {
  id: number;
  name: string;
  hash: string;
  isAdmin: number;
  banned: number;
  banReason: string | null;
  banExpires: number | null;
}

export interface RefreshToken {
  accountId: number;
  tokenHash: string;
  exp: number;
  created: number;
}

function hashToken(token: string) {
  return createHash("sha256").update(token).digest("hex");
}

const DB_CLEANUP_TIME = 1 * 60 * 60 * 24; // once per day

export class Db {
  readonly #db: Database.Database;

  // ==== prepared statements ==== //
  readonly #getByName: Database.Statement<[string], Account>;
  readonly #getById: Database.Statement<[number], Account>;
  readonly #insert: Database.Statement<[string, string]>;
  readonly #insertRefreshToken: Database.Statement<[number, string, number, number]>;
  readonly #deleteRefreshToken: Database.Statement<[string]>;
  readonly #getRefreshToken: Database.Statement<[string]>;
  readonly #ban: Database.Statement<[string | null, number | null, string]>;
  readonly #unban: Database.Statement<[string]>;

  readonly #cleanupInterval: NodeJS.Timeout;

  // path is a file, or ":memory:" for tests.
  constructor(path: string) {
    this.#db = new Database(path);
    this.#db.pragma("foreign_keys = ON");

    this.#db.exec(`
      CREATE TABLE IF NOT EXISTS accounts(
        id         INTEGER PRIMARY KEY,
        name       TEXT NOT NULL UNIQUE COLLATE NOCASE,
        hash       TEXT NOT NULL,
        isAdmin    INTEGER NOT NULL DEFAULT 0,
        banned     INTEGER NOT NULL DEFAULT 0,
        banReason  TEXT,
        banExpires INTEGER
      );
    `);

    this.#db.exec(`
      CREATE TABLE IF NOT EXISTS refresh_tokens (
        accountId  INTEGER NOT NULL,
        tokenHash  TEXT NOT NULL UNIQUE,
        exp        INTEGER NOT NULL,
        created    INTEGER NOT NULL,
        FOREIGN KEY (accountId) REFERENCES accounts (id) ON DELETE CASCADE
      );
    `);

    // The table must exist before these are prepared.
    this.#getByName = this.#db.prepare("SELECT * FROM accounts WHERE name = ?");
    this.#getById = this.#db.prepare("SELECT * FROM accounts WHERE id = ?");
    this.#insert = this.#db.prepare("INSERT INTO accounts(name, hash) VALUES (?, ?)");
    this.#insertRefreshToken = this.#db.prepare(
      "INSERT INTO refresh_tokens (accountId, tokenHash, exp, created) VALUES (?, ?, ?, ?)",
    );
    this.#deleteRefreshToken = this.#db.prepare("DELETE FROM refresh_tokens WHERE tokenHash = ?");
    this.#getRefreshToken = this.#db.prepare("SELECT * FROM refresh_tokens WHERE tokenHash = ?");
    this.#ban = this.#db.prepare(
      "UPDATE accounts SET banned = 1, banReason = ?, banExpires = ? WHERE name = ?",
    );
    this.#unban = this.#db.prepare(
      "UPDATE accounts SET banned = 0, banReason = NULL, banExpires = NULL WHERE name = ?",
    );

    const deleteExpired = this.#db.prepare(
      "DELETE FROM refresh_tokens WHERE exp < unixepoch('now')",
    );

    // unref: this timer alone must not keep the process alive; close() stops it.
    this.#cleanupInterval = setInterval(() => deleteExpired.run(), DB_CLEANUP_TIME * 1000).unref();
  }

  // ==== accounts ==== //
  getByName(name: string): Account | undefined {
    return this.#getByName.get(name);
  }

  getById(id: number): Account | undefined {
    return this.#getById.get(id);
  }

  // Returns the new id, or null if the name is taken (any letter case).
  createAccount(name: string, hash: string): number | null {
    try {
      return Number(this.#insert.run(name, hash).lastInsertRowid);
    } catch (err) {
      if (err instanceof Database.SqliteError && err.code === "SQLITE_CONSTRAINT_UNIQUE")
        return null;
      throw err;
    }
  }

  // expires is a unix time in seconds, null = permanent; false if no such account.
  setBan(name: string, reason: string | null, expires: number | null): boolean {
    return this.#ban.run(reason, expires, name).changes > 0;
  }

  clearBan(name: string): boolean {
    return this.#unban.run(name).changes > 0;
  }

  // ==== tokens ==== //
  createSession(id: number, nowSeconds: number, expires: number): string {
    const token = randomBytes(32).toString("base64url");
    const tokenHash = hashToken(token);
    this.#insertRefreshToken.run(id, tokenHash, expires, nowSeconds);
    return token;
  }

  deleteSession(token: string): boolean {
    return this.#deleteRefreshToken.run(hashToken(token)).changes > 0;
  }

  getSession(token: string, now: number): RefreshToken | undefined {
    const refreshToken = this.#getRefreshToken.get(hashToken(token)) as RefreshToken | undefined;
    if (!refreshToken || refreshToken.exp <= now) return undefined;

    return refreshToken;
  }

  // Swaps a valid session for a new one for the same account; undefined if the token is unknown or expired.
  // No awaits anywhere in here: the swap runs as one step. The delete's row count is the real guard, so
  // of two requests presenting the same token only one gets a new one. The transaction makes it all-or-nothing.
  rotateSession(
    token: string,
    now: number,
    expires: number,
  ): { accountId: number; token: string } | undefined {
    return this.#db.transaction(() => {
      const old = this.getSession(token, now);
      if (!old || !this.deleteSession(token)) return undefined;

      return { accountId: old.accountId, token: this.createSession(old.accountId, now, expires) };
    })();
  }

  close(): void {
    clearInterval(this.#cleanupInterval);
    this.#db.close();
  }
}
