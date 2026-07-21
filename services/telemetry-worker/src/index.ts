interface Env {
  DB: D1Database;
  SCHEMA_VERSION?: string;
}

const MAX_BODY_BYTES = 64 * 1024;
const MAX_EVENTS_PER_BATCH = 50;
const MAX_EVENT_BYTES = 8 * 1024;

const COMMON_FIELDS = new Set([
  "event_name",
  "event_version",
  "occurred_at",
  "app_version",
  "installation_id",
  "session_id",
  "os_name",
  "os_major_version",
  "architecture",
  "language",
  "consent_state"
]);

const REQUIRED_COMMON_FIELDS = [
  "event_name",
  "event_version",
  "occurred_at",
  "app_version",
  "installation_id",
  "session_id",
  "os_name",
  "consent_state"
];

const EVENT_FIELDS: Record<string, string[]> = {
  app_opened: ["launch_result"],
  startup_check_completed: ["overall_state", "error_code", "duration_ms"],
  language_selected: ["selected_language", "source"],
  telemetry_consent_changed: ["new_consent_state", "source"],
  scan_started: ["scan_id"],
  tool_check_completed: ["scan_id", "tool", "state", "error_code", "duration_ms"],
  scan_completed: ["scan_id", "overall_state", "missing_required_count", "unusable_required_count", "unsupported_count", "duration_ms"],
  repair_plan_created: ["scan_id", "repairable_count", "manual_count", "requires_uac_possible"],
  repair_started: ["repair_id", "tool", "method"],
  repair_step_completed: ["repair_id", "tool", "result", "error_code", "duration_ms"],
  repair_completed: ["repair_id", "overall_state", "completed_count", "failed_count", "duration_ms"],
  verification_started: ["verification_id"],
  verification_completed: ["verification_id", "overall_state", "error_code", "duration_ms"],
  ready_reached: ["verified_tool_count", "duration_from_open_ms"]
};

const DISALLOWED_FIELD_SUBSTRINGS = [
  "full_path",
  "path",
  "folder_path",
  "username",
  "account_name",
  "environment_variable",
  "env",
  "project_name",
  "project_content",
  "source_code",
  "prompt",
  "file_content",
  "directory_listing",
  "email",
  "secret",
  "api_key",
  "token",
  "credential",
  "cookie",
  "authorization_header",
  "raw_command_output",
  "raw_log",
  "crash_dump",
  "screenshot",
  "clipboard"
];

const UUID_RE = /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
const EMAIL_RE = /[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}/i;
const WINDOWS_PATH_RE = /[A-Za-z]:\\|\\\\[^\\]+\\/;
const TOKEN_RE = /(ghp_|github_pat_|Bearer\s+|api[_-]?key|secret|token)/i;

function jsonResponse(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": "no-store"
    }
  });
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function validateTimestamp(value: unknown): string | null {
  if (typeof value !== "string") {
    return "occurred_at must be a string";
  }
  const timestamp = Date.parse(value);
  if (!Number.isFinite(timestamp)) {
    return "occurred_at must be ISO-8601";
  }
  const now = Date.now();
  const oldest = now - 30 * 24 * 60 * 60 * 1000;
  const newest = now + 10 * 60 * 1000;
  if (timestamp < oldest || timestamp > newest) {
    return "occurred_at is outside the accepted window";
  }
  return null;
}

function rejectSensitiveValue(value: unknown): string | null {
  if (typeof value === "string") {
    if (EMAIL_RE.test(value)) return "value contains an email address";
    if (WINDOWS_PATH_RE.test(value)) return "value contains a local path";
    if (TOKEN_RE.test(value)) return "value contains a token-like string";
  }
  if (Array.isArray(value)) {
    for (const item of value) {
      const error = rejectSensitiveValue(item);
      if (error) return error;
    }
  }
  if (isRecord(value)) {
    for (const [key, nested] of Object.entries(value)) {
      const fieldError = rejectSensitiveField(key);
      if (fieldError) return fieldError;
      const nestedError = rejectSensitiveValue(nested);
      if (nestedError) return nestedError;
    }
  }
  return null;
}

function rejectSensitiveField(key: string): string | null {
  const normalized = key.toLowerCase();
  if (DISALLOWED_FIELD_SUBSTRINGS.some((term) => normalized.includes(term))) {
    return `field is disallowed: ${key}`;
  }
  return null;
}

function allowedFieldsForEvent(eventName: string): Set<string> {
  return new Set([...COMMON_FIELDS, ...(EVENT_FIELDS[eventName] ?? [])]);
}

function validateEvent(event: unknown): { ok: true; event: Record<string, unknown> } | { ok: false; error: string } {
  if (!isRecord(event)) {
    return { ok: false, error: "event must be an object" };
  }

  const eventName = event.event_name;
  if (typeof eventName !== "string" || !(eventName in EVENT_FIELDS)) {
    return { ok: false, error: "unknown event_name" };
  }

  for (const field of REQUIRED_COMMON_FIELDS) {
    if (!(field in event)) {
      return { ok: false, error: `missing required field: ${field}` };
    }
  }

  const timestampError = validateTimestamp(event.occurred_at);
  if (timestampError) return { ok: false, error: timestampError };

  if (event.event_version !== 1) return { ok: false, error: "event_version must be 1" };
  if (event.os_name !== "windows") return { ok: false, error: "os_name must be windows" };
  if (event.consent_state !== "enabled") return { ok: false, error: "consent_state must be enabled" };
  if (typeof event.installation_id !== "string" || !UUID_RE.test(event.installation_id)) {
    return { ok: false, error: "installation_id must be a UUID" };
  }
  if (typeof event.session_id !== "string" || !UUID_RE.test(event.session_id)) {
    return { ok: false, error: "session_id must be a UUID" };
  }

  const allowedFields = allowedFieldsForEvent(eventName);
  for (const [key, value] of Object.entries(event)) {
    const fieldError = rejectSensitiveField(key);
    if (fieldError) return { ok: false, error: fieldError };
    if (!allowedFields.has(key)) return { ok: false, error: `unknown field: ${key}` };
    const valueError = rejectSensitiveValue(value);
    if (valueError) return { ok: false, error: valueError };
  }

  const encoded = JSON.stringify(event);
  if (new TextEncoder().encode(encoded).byteLength > MAX_EVENT_BYTES) {
    return { ok: false, error: "event is too large" };
  }

  return { ok: true, event };
}

function nullableString(value: unknown): string | null {
  return typeof value === "string" && value.length > 0 ? value : null;
}

function nullableNumber(value: unknown): number | null {
  return typeof value === "number" && Number.isFinite(value) ? Math.trunc(value) : null;
}

function nullableBooleanInt(value: unknown): number | null {
  return typeof value === "boolean" ? (value ? 1 : 0) : nullableNumber(value);
}

function payloadRemainder(event: Record<string, unknown>): string {
  const known = new Set([
    "event_name",
    "event_version",
    "occurred_at",
    "app_version",
    "installation_id",
    "session_id",
    "os_name",
    "os_major_version",
    "architecture",
    "language",
    "consent_state",
    "scan_id",
    "repair_id",
    "verification_id",
    "tool",
    "state",
    "error_code",
    "overall_state",
    "duration_ms",
    "missing_required_count",
    "unusable_required_count",
    "unsupported_count",
    "repairable_count",
    "manual_count",
    "requires_uac_possible",
    "method",
    "result",
    "completed_count",
    "failed_count",
    "verified_tool_count",
    "duration_from_open_ms",
    "launch_result",
    "selected_language",
    "source",
    "new_consent_state"
  ]);
  const remainder: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(event)) {
    if (!known.has(key)) {
      remainder[key] = value;
    }
  }
  return JSON.stringify(remainder);
}

async function insertEvents(db: D1Database, events: Record<string, unknown>[]): Promise<void> {
  const receivedAt = new Date().toISOString();
  const statements = events.map((event) => db.prepare(
    `INSERT INTO events (
      id, event_name, event_version, occurred_at, received_at, app_version,
      installation_id, session_id, os_name, os_major_version, architecture,
      language, consent_state, scan_id, repair_id, verification_id, tool,
      state, error_code, overall_state, duration_ms, missing_required_count,
      unusable_required_count, unsupported_count, repairable_count, manual_count,
      requires_uac_possible, method, result, completed_count, failed_count,
      verified_tool_count, duration_from_open_ms, launch_result,
      selected_language, source, new_consent_state, payload_json
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`
  ).bind(
    crypto.randomUUID(),
    event.event_name,
    event.event_version,
    event.occurred_at,
    receivedAt,
    event.app_version,
    event.installation_id,
    event.session_id,
    event.os_name,
    nullableString(event.os_major_version),
    nullableString(event.architecture),
    nullableString(event.language),
    event.consent_state,
    nullableString(event.scan_id),
    nullableString(event.repair_id),
    nullableString(event.verification_id),
    nullableString(event.tool),
    nullableString(event.state),
    nullableString(event.error_code),
    nullableString(event.overall_state),
    nullableNumber(event.duration_ms),
    nullableNumber(event.missing_required_count),
    nullableNumber(event.unusable_required_count),
    nullableNumber(event.unsupported_count),
    nullableNumber(event.repairable_count),
    nullableNumber(event.manual_count),
    nullableBooleanInt(event.requires_uac_possible),
    nullableString(event.method),
    nullableString(event.result),
    nullableNumber(event.completed_count),
    nullableNumber(event.failed_count),
    nullableNumber(event.verified_tool_count),
    nullableNumber(event.duration_from_open_ms),
    nullableString(event.launch_result),
    nullableString(event.selected_language),
    nullableString(event.source),
    nullableString(event.new_consent_state),
    payloadRemainder(event)
  ));
  await db.batch(statements);
}

async function handleBatch(request: Request, env: Env): Promise<Response> {
  const contentType = request.headers.get("content-type") ?? "";
  if (!contentType.toLowerCase().includes("application/json")) {
    return jsonResponse({ ok: false, error: "content-type must be application/json" }, 415);
  }

  const bodyText = await request.text();
  if (new TextEncoder().encode(bodyText).byteLength > MAX_BODY_BYTES) {
    return jsonResponse({ ok: false, error: "request body is too large" }, 413);
  }

  let body: unknown;
  try {
    body = JSON.parse(bodyText);
  } catch {
    return jsonResponse({ ok: false, error: "invalid json" }, 400);
  }

  if (!isRecord(body) || !Array.isArray(body.events)) {
    return jsonResponse({ ok: false, error: "body must be { events: [...] }" }, 400);
  }
  if (body.events.length === 0 || body.events.length > MAX_EVENTS_PER_BATCH) {
    return jsonResponse({ ok: false, error: "invalid event batch size" }, 400);
  }

  const validEvents: Record<string, unknown>[] = [];
  for (const item of body.events) {
    const validated = validateEvent(item);
    if (!validated.ok) {
      return jsonResponse({ ok: false, error: validated.error }, 400);
    }
    validEvents.push(validated.event);
  }

  await insertEvents(env.DB, validEvents);
  return jsonResponse({ ok: true, accepted: validEvents.length });
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);
    if (request.method === "GET" && url.pathname === "/healthz") {
      return jsonResponse({
        ok: true,
        service: "vibeready-telemetry",
        schema_version: env.SCHEMA_VERSION ?? "1",
        d1_binding: Boolean(env.DB)
      });
    }
    if (request.method === "POST" && url.pathname === "/v1/telemetry/batch") {
      try {
        return await handleBatch(request, env);
      } catch {
        return jsonResponse({ ok: false, error: "telemetry service error" }, 500);
      }
    }
    return jsonResponse({ ok: false, error: "not found" }, 404);
  }
};
