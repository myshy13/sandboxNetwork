import { Db } from "../Db/db.js";
import { dbPath } from "../Db/dbPath.js";

const db = new Db(dbPath);

const name = process.argv[2];
if (!name) {
  console.log("Usage: unban.ts <name>");
  process.exit(-1);
}

db.clearBan(name);

if (db.clearBan(name)) {
  console.log("Cleared ban '" + name + "' successfully");
} else {
  console.error("Failed to ban account '" + name + "'");
}

db.close();