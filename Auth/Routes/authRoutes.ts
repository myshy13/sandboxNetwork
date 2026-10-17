import type { TypeBoxTypeProvider } from "@fastify/type-provider-typebox";
import { Type } from "@sinclair/typebox";
import { Value } from "@sinclair/typebox/value";
import { FastifyInstance } from "fastify";
import { Db, type Account } from "../Db/db.js";
import { Passwords } from "../Passwords/passwords.js";
import { TicketSigner } from "../Tickets/ticketSigner.js";

// this list will expand as more players join
const bannedNames: string[] = ["guest", "admin", "staff", "owner", " "];

function checkName(name: string) {
  name = name.toLowerCase();
  return !bannedNames.some((bannedName) => name.includes(bannedName));
}

const REFRESH_LIFETIME_SECONDS = 1 * 60 * 60 * 24 * 30;

class AuthRoutes {
  readonly #db: Db;
  readonly #passwords: Passwords;
  readonly #signer: TicketSigner;
  readonly #registerBody = Type.Object(
    {
      name: Type.String({ pattern: "^[A-Za-z0-9_]{3,16}$" }),
      password: Type.String({ minLength: 8, maxLength: 128 }),
    },
    { additionalProperties: false },
  );
  readonly #loginBody = Type.Object(
    {
      name: Type.String({ minLength: 1, maxLength: 16 }),
      password: Type.String({ minLength: 1, maxLength: 128 }),
    },
    { additionalProperties: false },
  );
  readonly #refreshBody = Type.Object(
    {
      refreshToken: Type.String({ pattern: "^[A-Za-z0-9_-]{43}$" }),
    },
    { additionalProperties: false },
  );
  readonly #logoutBody = Type.Object(
    {
      refreshToken: Type.String({ pattern: "^[A-Za-z0-9_-]{43}$" }),
    },
    { additionalProperties: false },
  );
  constructor(db: Db, passwords: Passwords, signer: TicketSigner) {
    this.#db = db;
    this.#signer = signer;
    this.#passwords = passwords;
  }

  // Banned while the flag is set and the ban is permanent (null) or ends in the future; no time units mixed.
  #isBanned(account: Account, now: number): boolean {
    return account.banned !== 0 && (account.banExpires === null || account.banExpires > now);
  }

  // Claims always come from the account row, never from anything the client sent.
  #ticketFor(account: Account, now: number): string {
    return this.#signer.sign(
      { id: account.id, name: account.name, isAdmin: account.isAdmin !== 0 },
      now,
    );
  }

  register(app: FastifyInstance) {
    // Same server, but request.body is typed from the schemas below.
    const typed = app.withTypeProvider<TypeBoxTypeProvider>();

    // Per account name (case-insensitive), 10 per 15 minutes; called by hand inside /login.
    const checkNameLimit = app.createRateLimit({
      max: 10,
      timeWindow: 15 * 60 * 1000,
      keyGenerator: (req) =>
        Value.Check(this.#loginBody, req.body) ? `name:${req.body.name.toLowerCase()}` : req.ip,
    });

    typed.post(
      "/register",
      {
        schema: { body: this.#registerBody },
        config: {
          rateLimit: { max: 5, timeWindow: "1 hour" },
        },
      },
      async (req, res) => {
        if (!checkName(req.body.name)) {
          res.status(400).send({ error: "Name not allowed" });
          return;
        }
        const hash = await this.#passwords.hash(req.body.password);
        const id = this.#db.createAccount(req.body.name, hash);
        if (id === null) {
          res.status(409).send({ error: "Name already taken" });
          return;
        }
        res.status(201).send({
          ticket: this.#signer.sign(
            { id, name: req.body.name, isAdmin: false },
            Math.floor(Date.now() / 1000),
          ),
        });
      },
    );

    typed.post(
      "/login",
      {
        schema: {
          body: this.#loginBody,
        },
        config: {
          rateLimit: { max: 10, timeWindow: "1 minute" },
        },
      },
      async (req, res) => {
        // Second limit, per account name. A manual check, because a limiter added as a hook is skipped
        // once the per-IP one above has run.
        const limit = await checkNameLimit(req);
        if (!limit.isAllowed && limit.isExceeded) {
          res
            .status(429)
            .header("retry-after", limit.ttlInSeconds)
            .send({ error: "Too many attempts" });
          return;
        }

        const now = Math.floor(Date.now() / 1000);
        const account = this.#db.getByName(req.body.name);

        // One verify per request: an unknown name (undefined hash) gets the dummy check inside it.
        const passwordOk = await this.#passwords.verify(account?.hash, req.body.password);
        if (!account || !passwordOk) {
          res.status(401).send({ error: "Invalid username or password" });
          return;
        }

        // Only reached with the right password, so the ban is never revealed to a stranger.
        if (this.#isBanned(account, now)) {
          res.status(403).send({
            error: "Account banned",
            reason: account.banReason,
            expires: account.banExpires,
          });
          return;
        }

        const ticket = this.#ticketFor(account, now);
        const refreshToken = this.#db.createSession(
          account.id,
          now,
          now + REFRESH_LIFETIME_SECONDS,
        );
        res.status(200).send({ ticket, refreshToken });
      },
    );

    typed.post(
      "/refresh",
      {
        schema: { body: this.#refreshBody },
        config: {
          rateLimit: { max: 30, timeWindow: "1 minute" },
        },
      },
      // Deliberately not async: no await between finding the session and swapping it, so nothing can
      // run in between. If an await is ever needed here, rotateSession's delete count still decides.
      (req, res) => {
        const now = Math.floor(Date.now() / 1000);

        // Unknown, expired and already-used tokens all get the same answer.
        const session = this.#db.getSession(req.body.refreshToken, now);
        const account = session && this.#db.getById(session.accountId);
        if (!account) {
          res.status(401).send({ error: "Invalid refresh token" });
          return;
        }

        // Checked before rotating, so a banned player's token is not used up.
        if (this.#isBanned(account, now)) {
          res.status(403).send({
            error: "Account banned",
            reason: account.banReason,
            expires: account.banExpires,
          });
          return;
        }

        const rotated = this.#db.rotateSession(
          req.body.refreshToken,
          now,
          now + REFRESH_LIFETIME_SECONDS,
        );
        if (!rotated) {
          res.status(401).send({ error: "Invalid refresh token" });
          return;
        }

        res
          .status(200)
          .send({ ticket: this.#ticketFor(account, now), refreshToken: rotated.token });
      },
    );

    typed.post(
      "/logout",
      {
        schema: {
          body: this.#logoutBody,
        },
        config: {
          rateLimit: { max: 10, timeWindow: "1 minute" },
        },
      },
      async (req, res) => {
        const token = req.body.refreshToken;
        this.#db.deleteSession(token);
        return res.status(204).send();
      },
    );
  }
}

export default AuthRoutes;
