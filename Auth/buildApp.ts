import helmet from "@fastify/helmet";
import fastifyCors from "@fastify/cors";
import fastifyRateLimit from "@fastify/rate-limit";
import Fastify from "fastify";
import { Db } from "./Db/db.js";
import { Passwords } from "./Passwords/passwords.js";
import { TicketSigner } from "./Tickets/ticketSigner.js";
import AuthRoutes from "./Routes/authRoutes.js";
import AdminRoutes from "./Routes/adminRoutes.js";
import { sign } from "node:crypto";

async function buildApp(
  db: Db,
  passwords: Passwords,
  ticketSigner: TicketSigner,
  corsOrigin: string | undefined = process.env.CORS_ORIGIN,
) {
  const app = Fastify({ trustProxy: true, bodyLimit: 1024 });

  await app.register(helmet);
  await app.register(fastifyCors, { origin: corsOrigin || false });
  await app.register(fastifyRateLimit, { max: 100, timeWindow: "1 minute" });

  app.addHook("onClose", () => db.close());

  // link routes
  new AuthRoutes(db, passwords, ticketSigner).register(app);
  new AdminRoutes(db, passwords, ticketSigner).register(app);

  return app;
}

export default buildApp;
