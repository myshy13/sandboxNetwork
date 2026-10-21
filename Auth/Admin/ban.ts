import { Db } from "../Db/db.js";
import { dbPath } from "../Db/dbPath.js";

const db = new Db(dbPath);

const [name, reason, hours] = process.argv.slice(2) as [name: string, reason: string, time: number];
if (!name || !reason) {
  console.log("Usage: ban.ts <name> <reason> (Optional)<hours>");
  process.exit(-1);
}
const currentTime = new Date().getTime() / 1000;

let banExp: number | null;
if (hours && !Number.isNaN(hours)) {
  banExp = currentTime + hours * 60 * 60;
} else {
  banExp = null;
}

if (db.setBan(name, reason, banExp)) {
  console.log("Banned '" + name + "' for " + reason + "successfully");
} else {
  console.error("Failed to bann account '" + name + "'");
}

db.close();
