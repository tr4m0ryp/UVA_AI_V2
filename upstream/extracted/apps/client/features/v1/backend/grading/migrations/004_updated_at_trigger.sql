CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
  NEW.updated_at = now();
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER set_grading_sessions_updated_at
  BEFORE UPDATE ON grading_sessions
  FOR EACH ROW
  EXECUTE FUNCTION update_updated_at_column();
