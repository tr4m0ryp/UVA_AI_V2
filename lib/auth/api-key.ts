import { NextRequest, NextResponse } from "next/server";
import crypto from "crypto";
import { createServerClient } from "../db/client";

export function hashKey(key: string): string {
  return crypto.createHash("sha256").update(key).digest("hex");
}

export interface ResolvedKey {
  apiKey: {
    id: string;
    model: string;
    [key: string]: unknown;
  };
  uvaSession: string;
  db: ReturnType<typeof createServerClient>;
}

type AuthResult =
  | { ok: true; data: ResolvedKey }
  | { ok: false; response: NextResponse };

export async function resolveBearerKey(req: NextRequest): Promise<AuthResult> {
  const auth = req.headers.get("authorization") ?? "";
  const bearerMatch = auth.match(/^Bearer\s+(.+)$/i);
  if (!bearerMatch) {
    return { ok: false, response: NextResponse.json({ error: "Missing Bearer token" }, { status: 401 }) };
  }
  const rawKey = bearerMatch[1].trim();
  const keyHash = hashKey(rawKey);

  const db = createServerClient();

  const { data: apiKey, error: keyErr } = await db
    .from("api_keys")
    .select("*, users(uva_session)")
    .eq("key_hash", keyHash)
    .eq("is_active", true)
    .single();

  if (keyErr || !apiKey) {
    return { ok: false, response: NextResponse.json({ error: "Invalid or inactive API key" }, { status: 401 }) };
  }

  const uvaSession = (apiKey.users as { uva_session: string } | null)?.uva_session ?? "";
  if (!uvaSession) {
    return {
      ok: false,
      response: NextResponse.json(
        { error: "UvA session cookie not configured. Visit /dashboard/settings." },
        { status: 503 }
      ),
    };
  }

  return { ok: true, data: { apiKey, uvaSession, db } };
}

export function logApiRequest(
  db: ReturnType<typeof createServerClient>,
  apiKeyId: string,
  model: string,
  startTime: number,
  inputChars: number,
  outputChars: number,
  success: boolean
): void {
  void Promise.resolve(
    db.from("request_logs").insert({
      api_key_id: apiKeyId,
      model,
      input_chars: inputChars,
      output_chars: outputChars,
      success,
      latency_ms: Date.now() - startTime,
    })
  )
    .then(() =>
      Promise.resolve(
        db
          .from("api_keys")
          .update({ last_used: new Date().toISOString() })
          .eq("id", apiKeyId)
      )
    )
    .catch(() => {});
}
