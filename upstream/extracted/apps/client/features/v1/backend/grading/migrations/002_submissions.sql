CREATE TABLE submissions (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  session_id UUID NOT NULL REFERENCES grading_sessions(id) ON DELETE CASCADE,
  student_identifier TEXT NOT NULL,
  file_path TEXT NOT NULL,
  content_extracted TEXT,
  status TEXT NOT NULL DEFAULT 'pending'
    CHECK (status IN ('pending','grading','completed','re_grading','failed')),
  is_anchor BOOLEAN NOT NULL DEFAULT false,
  anchor_level TEXT CHECK (anchor_level IN ('HIGH','MID','LOW')),
  scores JSONB,
  total_score NUMERIC,
  feedback TEXT,
  improvement_comments TEXT,
  reasoning TEXT,
  graded_at TIMESTAMPTZ,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_submissions_session_id ON submissions(session_id);
