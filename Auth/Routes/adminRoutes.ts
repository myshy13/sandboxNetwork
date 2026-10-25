import { Type, TypeBoxTypeProvider } from "@fastify/type-provider-typebox";
import { Db } from "../Db/db.js";
import { TicketSigner } from "../Tickets/ticketSigner.js";
import { FastifyInstance, FastifyReply, FastifyRequest } from "fastify";

class AdminRoutes {
  readonly #db: Db;
  readonly #signer: TicketSigner;

  readonly #banPlayerBody = Type.Object(
    {
      name: Type.String({ minLength: 1, maxLength: 16 }),
      reason: Type.String({ minLength: 1, maxLength: 64 }),
      // Omit for permanent; a Null union lets Ajv coerce 0 to null and ban forever. Max 1 year.
      hours: Type.Optional(Type.Integer({ minimum: 1, maximum: 8760 })),
    },
    { additionalProperties: false },
  );

  readonly #unbanPlayerBody = Type.Object(
    {
      name: Type.String({ minLength: 1, maxLength: 16 }),
    },
    { additionalProperties: false },
  );

  constructor(db: Db, signer: TicketSigner) {
    this.#db = db;
    this.#signer = signer;
  }

  // Runs before body validation, so a caller who isn't an admin learns nothing about the schema.
  readonly #requireAdmin = async (req: FastifyRequest, res: FastifyReply) => {
    const header = req.headers.authorization ?? "";
    // An empty token fails verify, so there is no separate check for it.
    const claim = header.startsWith("Bearer ")
      ? this.#signer.verify(header.slice(7), Date.now() / 1000)
      : null;
    if (!claim) return res.status(401).send({ error: "Ticket invalid" });

    // The ticket can be an hour old, so the db decides who is an admin right now.
    const account = this.#db.getById(claim.id);
    if (!account) return res.status(401).send({ error: "Account doesn't exist" });
    if (!account.isAdmin) return res.status(403).send({ error: "Not an admin" });
    if (account.banned) return res.status(403).send({ error: "Admin account banned" });
  };

  register(app: FastifyInstance) {
    // One scope for every admin route: the hook can't be forgotten on the next one.
    app.register(async (scope) => {
      scope.addHook("onRequest", this.#requireAdmin);
      const typed = scope.withTypeProvider<TypeBoxTypeProvider>();

      interface NameParams {
        name: string | null;
      }

      typed.get<{ Params: NameParams }>("/admin/getban/:name", async (req, res) => {
        const { name } = req.params;
        if (!name) return res.status(400).send({ error: "Name required" });
        const account = this.#db.getByName(name);
        if (!account) return res.status(404).send({ error: "Account not found" });
        if (!account.banned) return res.status(200).send({ banned: false });
        return res.status(200).send({ banned: true, exp: account.banExpires, reason: account.banReason });
      });

      typed.post("/admin/ban", { schema: { body: this.#banPlayerBody } }, async (req, res) => {
        const { name, reason, hours } = req.body;
        const expires = hours ? Math.floor(Date.now() / 1000) + hours * 3600 : null;

        if (this.#db.setBan(name, reason, expires)) return res.status(204).send();
        else return res.status(404).send({ error: "Player not found" });
      });

      typed.post("/admin/unban", { schema: { body: this.#unbanPlayerBody } }, async (req, res) => {
        const { name } = req.body;

        if (this.#db.clearBan(name)) return res.status(204).send();
        else return res.status(404).send({ error: "Player not found" });
      });
    });
  }
}

export default AdminRoutes;
