import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

const source = readFileSync(new URL("../src/index.ts", import.meta.url), "utf8");
const migration = readFileSync(new URL("../migrations/0001_events.sql", import.meta.url), "utf8");
const packageJson = JSON.parse(readFileSync(new URL("../package.json", import.meta.url), "utf8"));
const wranglerConfig = JSON.parse(readFileSync(new URL("../wrangler.jsonc", import.meta.url), "utf8"));

test("worker exposes required routes", () => {
  assert.match(source, /\/healthz/);
  assert.match(source, /\/v1\/telemetry\/batch/);
});

test("worker rejects sensitive fields and bad events", () => {
  for (const term of ["full_path", "folder_path", "username", "email", "token", "raw_log"]) {
    assert.match(source, new RegExp(term));
  }
  assert.match(source, /unknown event_name/);
  assert.match(source, /request body is too large/);
});

test("migration creates typed events table and indexes", () => {
  assert.match(migration, /CREATE TABLE IF NOT EXISTS events/i);
  for (const column of ["event_name", "app_version", "installation_id", "error_code", "duration_ms"]) {
    assert.match(migration, new RegExp(column, "i"));
  }
  assert.match(migration, /idx_events_event_time/i);
  assert.match(migration, /idx_events_version_event_time/i);
});

test("migration scripts target the configured D1 binding", () => {
  assert.ok(wranglerConfig.d1_databases.some(({ binding }) => binding === "DB"));
  assert.equal(packageJson.scripts["migrate:local"], "wrangler d1 migrations apply DB --local");
  assert.equal(packageJson.scripts["migrate:remote"], "wrangler d1 migrations apply DB --remote");
});
