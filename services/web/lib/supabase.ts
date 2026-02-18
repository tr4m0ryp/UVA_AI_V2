import { createClient } from "@supabase/supabase-js";

const supabaseUrl = process.env.NEXT_PUBLIC_SUPABASE_URL!;
const supabaseAnonKey = process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!;
const supabaseServiceKey = process.env.SUPABASE_SERVICE_ROLE_KEY!;

export function createBrowserClient() {
  return createClient(supabaseUrl, supabaseAnonKey);
}

export function createServerClient() {
  return createClient(supabaseUrl, supabaseServiceKey, {
    auth: {
      autoRefreshToken: false,
      persistSession: false,
    },
  });
}

export type Database = {
  users: {
    id: string;
    email: string;
    name: string | null;
    uva_session: string;
    created_at: string;
    last_login: string;
  };
  api_keys: {
    id: string;
    user_id: string;
    key_hash: string;
    key_prefix: string;
    name: string;
    model: string;
    temperature: number;
    top_p: number;
    max_tokens: number;
    frequency_penalty: number;
    presence_penalty: number;
    reasoning_effort: string;
    system_prompt: string;
    allow_tools: boolean;
    tool_allowlist: string;
    max_tokens_tool: number;
    is_active: boolean;
    created_at: string;
    last_used: string | null;
  };
  grading_sessions: {
    id: string;
    user_id: string;
    title: string;
    submission_count: number;
    results_json: Record<string, unknown>;
    created_at: string;
    updated_at: string;
  };
  request_logs: {
    id: string;
    api_key_id: string;
    model: string;
    input_chars: number;
    output_chars: number;
    success: boolean;
    latency_ms: number;
    created_at: string;
  };
};
