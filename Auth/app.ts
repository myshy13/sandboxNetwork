import "dotenv/config";
import { join } from "node:path";
import buildApp from "./buildApp.js";
import { Db } from "./Db/db.js";
import { Passwords } from "./Passwords/passwords.js";
import { TicketSigner } from "./Tickets/ticketSigner.js";

let dbPath = process.env.AUTH_DB_PATH;
let port = process.env.PORT as number | undefined;
let host = process.env.HOST;

// defaults
if (!dbPath) dbPath = join(import.meta.dirname, "app.db");
if (!port) port = 3200;
if (!host) host = "127.0.0.1";

const db = new Db(dbPath);
const passwords = new Passwords();
const ticketSigner = new TicketSigner();

const app = await buildApp(db, passwords, ticketSigner);
await app.listen({
  port: port,
  host: host,
});
