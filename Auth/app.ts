import "dotenv/config";
import buildApp from "./buildApp.js";
import { Db } from "./Db/db.js";
import { dbPath } from "./Db/dbPath.js";
import { Passwords } from "./Passwords/passwords.js";
import { TicketSigner } from "./Tickets/ticketSigner.js";

let port = process.env.PORT as number | undefined;
let host = process.env.HOST;

// defaults
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
