import Database from "better-sqlite3";

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

export class Db {
  readonly #db: Database.Database;

  // ==== prepared statements ==== //
  readonly #getByName: Database.Statement<[string], Account>;
  readonly #insert: Database.Statement<[string, string]>;
  readonly #ban: Database.Statement<[string | null, number | null, string]>;
  readonly #unban: Database.Statement<[string]>;

  // path is a file, or ":memory:" for tests.
  constructor(path: string) {
    this.#db = new Database(path);
    this.#db.exec(`
      CREATE TABLE IF NOT EXISTS accounts(
        id         INTEGER PRIMARY KEY,
        name       TEXT NOT NULL UNIQUE COLLATE NOCASE,
        hash       TEXT NOT NULL,
        isAdmin    INTEGER NOT NULL DEFAULT 0,
        banned     INTEGER NOT NULL DEFAULT 0,
        banReason  TEXT,
        banExpires INTEGER
      )
    `);

    // The table must exist before these are prepared.
    this.#getByName = this.#db.prepare("SELECT * FROM accounts WHERE name = ?");
    this.#insert = this.#db.prepare("INSERT INTO accounts(name, hash) VALUES (?, ?)");
    this.#ban = this.#db.prepare(
      "UPDATE accounts SET banned = 1, banReason = ?, banExpires = ? WHERE name = ?",
    );
    this.#unban = this.#db.prepare(
      "UPDATE accounts SET banned = 0, banReason = NULL, banExpires = NULL WHERE name = ?",
    );
  }

  // ==== accounts ==== //
  getByName(name: string): Account | undefined {
    return this.#getByName.get(name);
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

  close(): void {
    this.#db.close();
  }
}
