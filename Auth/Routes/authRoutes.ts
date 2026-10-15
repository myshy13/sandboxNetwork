import type { TypeBoxTypeProvider } from "@fastify/type-provider-typebox";
import { Type } from "@sinclair/typebox";
import { FastifyInstance } from "fastify";
import { Db } from "../Db/db.js";
import { Passwords } from "../Passwords/passwords.js";
import { TicketSigner } from "../Tickets/ticketSigner.js";

// this list will expand as more players join
const bannedNames: string[] = ["guest", "admin", "staff", "owner", " "];

function checkName(name: string) {
  name = name.toLowerCase();
  bannedNames.forEach((bannedName) => {
    if (name.includes(bannedName)) return false;
  });
  return true;
}

class AuthRoutes {
  readonly #db: Db;
  readonly #passwords: Passwords;
  readonly #signer: TicketSigner;
  constructor(db: Db, passwords: Passwords, signer: TicketSigner) {
    this.#db = db;
    this.#signer = signer;
    this.#passwords = passwords;
  }

  register(app: FastifyInstance) {
    const RegisterBody = Type.Object(
      {
        name: Type.String({ pattern: "^[A-Za-z0-9_]{3,16}$" }),
        password: Type.String({ minLength: 8, maxLength: 128 }),
      },
      { additionalProperties: false },
    );
    // Same server, but request.body is typed from the schemas below.
    const typed = app.withTypeProvider<TypeBoxTypeProvider>();

    typed.post(
      "/register",
      {
        schema: { body: RegisterBody },
        config: {
          rateLimit: { max: 5, timeWindow: "1 hour" },
        },
      },
      async (req, res) => {
        if (!checkName(req.body.name)) {
          res.status(400).send({ error: "Name not allowed" });
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
  }
}

export default AuthRoutes;
