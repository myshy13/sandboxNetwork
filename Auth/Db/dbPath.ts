import "dotenv/config";
import { join } from "node:path";

// One definition of the db file, so the server and the admin scripts can't open different ones.
export const dbPath = process.env.AUTH_DB_PATH || join(import.meta.dirname, "../app.db");
