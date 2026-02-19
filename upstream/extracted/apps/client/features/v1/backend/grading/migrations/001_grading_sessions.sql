CREATE TABLE grading_sessions (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  user_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  assignment_text TEXT NOT NULL DEFAULT '',
  assignment_file_path TEXT,
  rubric_raw TEXT NOT NULL DEFAULT '',
  rubric_structured JSONB,
  model_answer TEXT NOT NULL DEFAULT '',
  model_answer_file_path TEXT,
  selected_model TEXT,
  model_reasoning TEXT,
  status TEXT NOT NULL DEFAULT 'draft'
    CHECK (status IN ('draft','parsing_rubric','calibrating','grading','normalizing','completed','failed')),
  anchor_data JSONB,
  stats JSONB,
  total_submissions INTEGER NOT NULL DEFAULT 0,
  graded_count INTEGER NOT NULL DEFAULT 0,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_grading_sessions_user_id ON grading_sessions(user_id);
ALTER TABLE grading_sessions ENABLE ROW LEVEL SECURITY;
CREATE POLICY "Users manage own sessions" ON grading_sessions FOR ALL
  USING (auth.uid() = user_id) WITH CHECK (auth.uid() = user_id);
