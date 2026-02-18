import { NextRequest, NextResponse } from "next/server";
import { createServerClient } from "@/lib/db/client";
import { requireApiAuth } from "@/lib/auth/session";
import { isValidModel } from "@/lib/uva/models";
import crypto from "crypto";
import { v4 as uuidv4 } from "uuid";

function generateApiKey(): { raw: string; hash: string; prefix: string } {
  const raw = `uva-${uuidv4().replace(/-/g, "")}`;
  const hash = crypto.createHash("sha256").update(raw).digest("hex");
  const prefix = raw.slice(0, 12);
  return { raw, hash, prefix };
}

export async function GET() {
  const authResult = await requireApiAuth();
  if (authResult.type === "error") return authResult.response;
  const user = authResult.user;

  const db = createServerClient();
  const { data, error } = await db
    .from("api_keys")
    .select(
      "id,key_prefix,name,model,temperature,top_p,max_tokens,is_active,created_at,last_used,system_prompt,allow_tools,reasoning_effort"
    )
    .eq("user_id", user.id)
    .order("created_at", { ascending: false });

  if (error) {
    return NextResponse.json({ error: error.message }, { status: 500 });
  }

  return NextResponse.json(data);
}

export async function POST(req: NextRequest) {
  const authResult = await requireApiAuth();
  if (authResult.type === "error") return authResult.response;
  const user = authResult.user;

  let body: {
    name?: string;
    model?: string;
    temperature?: number;
    top_p?: number;
    max_tokens?: number;
    system_prompt?: string;
    reasoning_effort?: string;
  };

  try {
    body = await req.json();
  } catch {
    return NextResponse.json({ error: "Invalid JSON" }, { status: 400 });
  }

  const model = body.model ?? "gpt-4o";
  if (!isValidModel(model)) {
    return NextResponse.json({ error: `Unknown model: ${model}` }, { status: 400 });
  }

  const { raw, hash, prefix } = generateApiKey();
  const db = createServerClient();

  const { data, error } = await db
    .from("api_keys")
    .insert({
      user_id: user.id,
      key_hash: hash,
      key_prefix: prefix,
      name: body.name ?? "",
      model,
      temperature: body.temperature ?? 0.5,
      top_p: body.top_p ?? 0.5,
      max_tokens: body.max_tokens ?? 4096,
      system_prompt: body.system_prompt ?? "",
      reasoning_effort: body.reasoning_effort ?? "medium",
    })
    .select()
    .single();

  if (error || !data) {
    return NextResponse.json({ error: error?.message ?? "Insert failed" }, { status: 500 });
  }

  return NextResponse.json({ ...data, raw_key: raw }, { status: 201 });
}
