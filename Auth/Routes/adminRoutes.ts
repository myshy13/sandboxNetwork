import { Type, TypeBoxTypeProvider } from "@fastify/type-provider-typebox";
import { Db } from "../Db/db.js";
import { Passwords } from "../Passwords/passwords.js";
import { TicketSigner } from "../Tickets/ticketSigner.js";
import { FastifyInstance, FastifyReply, FastifyRequest } from "fastify";

class AdminRoutes {
  readonly #db: Db;
  readonly #passwords: Passwords;
  readonly #signer: TicketSigner;

  readonly #verifyTicket: (req: FastifyRequest, res: FastifyReply) => void;

  readonly #banPlayerBody = Type.Object(
    {
      name: Type.String({ minLength: 1, maxLength: 16 }),
    },
    { additionalProperties: false },
  );

  constructor(db: Db, passwords: Passwords, signer: TicketSigner) {
    this.#db = db;
    this.#signer = signer;
    this.#passwords = passwords;

    this.#verifyTicket = (req, res) => {}
  }

  register(app: FastifyInstance) {
    const typed = app.withTypeProvider<TypeBoxTypeProvider>();

    typed.post(
      "/status",
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
        if (!token) return res.status(400).send({ error: "Ticket invalid" });
        this.#signer.verify(token, new Date().getTime() / 1000)
      },
    );
  }
}
