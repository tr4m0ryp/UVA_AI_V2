import { NextRequest, NextResponse } from "next/server";
import { hashKey } from "@/lib/auth/api-key";
import { createServerClient } from "@/lib/db/client";

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "PUT, OPTIONS",
  "Access-Control-Allow-Headers": "Authorization, Content-Type",
};

export async function OPTIONS() {
  return new NextResponse(null, { status: 204, headers: CORS_HEADERS });
}

export async function PUT(req: NextRequest) {
  const auth = req.headers.get("authorization") ?? "";
  const bearerMatch = auth.match(/^Bearer\s+(.+)$/i);
  if (!bearerMatch) {
    return NextResponse.json(
      { error: "Missing Bearer token" },
      { status: 401, headers: CORS_HEADERS }
    );
  }

  const rawKey = bearerMatch[1].trim();
  const keyHash = hashKey(rawKey);
  const db = createServerClient();

  const { data: apiKey, error: keyErr } = await db
    .from("api_keys")
    .select("user_id")
    .eq("key_hash", keyHash)
    .eq("is_active", true)
    .single();

  if (keyErr || !apiKey) {
    return NextResponse.json(
      { error: "Invalid or inactive API key" },
      { status: 401, headers: CORS_HEADERS }
    );
  }

  let body: { uva_session?: string };
  try {
    body = await req.json();
  } catch {
    return NextResponse.json(
      { error: "Invalid JSON" },
      { status: 400, headers: CORS_HEADERS }
    );
  }

  if (typeof body.uva_session !== "string" || !body.uva_session.trim()) {
    return NextResponse.json(
      { error: "uva_session string is required" },
      { status: 400, headers: CORS_HEADERS }
    );
  }

  const { error: updateErr } = await db
    .from("users")
    .update({ uva_session: body.uva_session.trim() })
    .eq("id", apiKey.user_id);

  if (updateErr) {
    return NextResponse.json(
      { error: updateErr.message },
      { status: 500, headers: CORS_HEADERS }
    );
  }

  return NextResponse.json({ ok: true }, { headers: CORS_HEADERS });
}
