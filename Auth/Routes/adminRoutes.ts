import { Type, TypeBoxTypeProvider } from "@fastify/type-provider-typebox";
import { Db } from "../Db/db.js";
import { Passwords } from "../Passwords/passwords.js";
import { TicketSigner } from "../Tickets/ticketSigner.js";
import { FastifyInstance } from "fastify";

class AdminRoutes {
  readonly #db: Db;
  readonly #signer: TicketSigner;

  readonly #banPlayerBody = Type.Object(
    {
      name: Type.String({ minLength: 1, maxLength: 16 }),
      reason: Type.String({ minLength: 1, maxLength: 64 }),
      hours: Type.Union([Type.Integer(), Type.Null()]),
    },
    { additionalProperties: false },
  );

  readonly #authHeaders = Type.Object({
    authorization: Type.String(),
  });

  constructor(db: Db, signer: TicketSigner) {
    this.#db = db;
    this.#signer = signer;
  }

  register(app: FastifyInstance) {
    const typed = app.withTypeProvider<TypeBoxTypeProvider>();

    typed.post(
      "/admin/ban",
      {
        schema: {
          body: this.#banPlayerBody,
          headers: this.#authHeaders,
        },
      },
      async (req, res) => {
        const token = req.headers["authorization"];
        const player = req.body.name;
        const reason = req.body.reason;
        const hours = req.body.hours;
        if (!hours || hours < 0) return res.status(400).send({ error: "Or in the past" });
        const claim = this.#signer.verify(token, Date.now() / 1000);
        if (!claim) return res.status(400).send({ error: "Ticket invalid" });
        if (!claim.isAdmin) return res.status(401).send({ error: "Not an admin" });

        if (this.#db.setBan(player, reason, Math.floor(Date.now() / 1000) + hours * 3600))
          return res.status(204).send();
        else return res.status(404).send({ error: "Player not found" });
      },
    );
  }
}

export default AdminRoutes;
