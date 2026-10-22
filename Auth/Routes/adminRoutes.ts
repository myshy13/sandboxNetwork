import { Type, TypeBoxTypeProvider } from "@fastify/type-provider-typebox";
import { Db } from "../Db/db.js";
import { Passwords } from "../Passwords/passwords.js";
import { TicketSigner } from "../Tickets/ticketSigner.js";
import { FastifyInstance, FastifyReply, FastifyRequest } from "fastify";

class AdminRoutes {
  readonly #db: Db;
  readonly #passwords: Passwords;
  readonly #signer: TicketSigner;

  readonly #banPlayerBody = Type.Object(
    {
      name: Type.String({ minLength: 1, maxLength: 16 }),
      reason: Type.String({ minLength: 1, maxLength: 64 }),
      exp: Type.Union([Type.Number(), Type.Null()]),
    },
    { additionalProperties: false },
  );

  constructor(db: Db, passwords: Passwords, signer: TicketSigner) {
    this.#db = db;
    this.#signer = signer;
    this.#passwords = passwords;
  }

  register(app: FastifyInstance) {
    const typed = app.withTypeProvider<TypeBoxTypeProvider>();

    typed.post(
      "/admin/ban",
      {
        schema: {
          body: this.#banPlayerBody,
          headers: {
            Authorization: Type.String(),
          },
        },
      },
      async (req, res) => {
        const token = req.headers["authorization"];
        const player = req.body.name;
        const reason = req.body.reason;
        const expiry = req.body.exp;
        if (!player || !reason) return res.status(400).send({ error: "Invalid "})
        if (!token) return res.status(400).send({ error: "Ticket invalid" });
        const claim = this.#signer.verify(token, new Date().getTime() / 1000);
        if (!claim) return res.status(400).send({ error: "Ticket invalid" });
        if (!claim.isAdmin) res.status(401).send({ error: "Not an admin" });

        if (this.#db.setBan(player, reason, expiry)) {
          return res.status(204).send();
        } else {
          return res.status(500).send({ error: "Failed to ban player" })
        };
      },
    );
  }
}

export default AdminRoutes;