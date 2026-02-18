-- users: one row per authenticated user
CREATE TABLE IF NOT EXISTS users (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  email TEXT UNIQUE NOT NULL,
  name TEXT,
  uva_session TEXT NOT NULL DEFAULT '',
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  last_login TIMESTAMPTZ NOT NULL DEFAULT now()
);

ALTER TABLE users ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Users own row"
  ON users FOR ALL
  USING (auth.uid()::text = id::text);

-- api_keys: per-user API key records; raw key is never stored, only SHA-256 hash
CREATE TABLE IF NOT EXISTS api_keys (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  key_hash TEXT UNIQUE NOT NULL,
  key_prefix TEXT NOT NULL,
  name TEXT NOT NULL DEFAULT '',
  model TEXT NOT NULL,
  temperature REAL NOT NULL DEFAULT 0.5,
  top_p REAL NOT NULL DEFAULT 0.5,
  max_tokens INTEGER NOT NULL DEFAULT 4096,
  frequency_penalty REAL NOT NULL DEFAULT 0.0,
  presence_penalty REAL NOT NULL DEFAULT 0.0,
  reasoning_effort TEXT NOT NULL DEFAULT 'medium',
  system_prompt TEXT NOT NULL DEFAULT '',
  allow_tools BOOLEAN NOT NULL DEFAULT true,
  tool_allowlist TEXT NOT NULL DEFAULT '',
  max_tokens_tool INTEGER NOT NULL DEFAULT 8192,
  is_active BOOLEAN NOT NULL DEFAULT true,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  last_used TIMESTAMPTZ
);

ALTER TABLE api_keys ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Users own keys"
  ON api_keys FOR ALL
  USING (
    user_id IN (SELECT id FROM users WHERE auth.uid()::text = id::text)
  );

-- grading_sessions: assessment sessions with arbitrary JSON results
CREATE TABLE IF NOT EXISTS grading_sessions (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  title TEXT NOT NULL,
  submission_count INTEGER NOT NULL DEFAULT 0,
  results_json JSONB NOT NULL DEFAULT '{}',
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

ALTER TABLE grading_sessions ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Users own sessions"
  ON grading_sessions FOR ALL
  USING (
    user_id IN (SELECT id FROM users WHERE auth.uid()::text = id::text)
  );

-- request_logs: per-request telemetry; no direct user-level RLS (enforced via FK join)
CREATE TABLE IF NOT EXISTS request_logs (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  api_key_id UUID NOT NULL REFERENCES api_keys(id) ON DELETE CASCADE,
  model TEXT NOT NULL,
  input_chars INTEGER NOT NULL DEFAULT 0,
  output_chars INTEGER NOT NULL DEFAULT 0,
  success BOOLEAN NOT NULL DEFAULT true,
  latency_ms INTEGER NOT NULL DEFAULT 0,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_logs_key_created
  ON request_logs(api_key_id, created_at);
