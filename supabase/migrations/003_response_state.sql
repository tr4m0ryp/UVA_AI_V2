CREATE TABLE IF NOT EXISTS response_state (
  id TEXT PRIMARY KEY,
  api_key_id UUID NOT NULL REFERENCES api_keys(id) ON DELETE CASCADE,
  model TEXT NOT NULL,
  messages_json JSONB NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_resp_state_created ON response_state(created_at);
