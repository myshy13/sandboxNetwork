import { Db } from "../Db/db.js";
import { dbPath } from "../Db/dbPath.js";

const db = new Db(dbPath);

const [name, flag] = process.argv.slice(2);
if (!name || (flag !== undefined && flag !== "--revoke")) {
  console.log("Usage: promote.ts <name> [--revoke]");
  process.exit(-1);
}
const makeAdmin = flag !== "--revoke";

if (db.setAdmin(name, makeAdmin)) {
  console.log((makeAdmin ? "Made '" : "Removed admin from '") + name + "' successfully");
} else {
  console.error("No account named '" + name + "'");
}

db.close();
