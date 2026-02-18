import { NextRequest, NextResponse } from "next/server";
import { createServerClient } from "@/lib/db/client";
import { requireApiAuth } from "@/lib/auth/session";

export async function GET() {
  const authResult = await requireApiAuth();
  if (authResult.type === "error") return authResult.response;
  const user = authResult.user;

  const db = createServerClient();
  const { data, error } = await db
    .from("grading_sessions")
    .select("id,title,submission_count,created_at,updated_at")
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

  let body: { title?: string; results_json?: Record<string, unknown> };
  try {
    body = await req.json();
  } catch {
    return NextResponse.json({ error: "Invalid JSON" }, { status: 400 });
  }

  if (!body.title?.trim()) {
    return NextResponse.json({ error: "title is required" }, { status: 400 });
  }

  const db = createServerClient();
  const { data, error } = await db
    .from("grading_sessions")
    .insert({
      user_id: user.id,
      title: body.title.trim(),
      results_json: body.results_json ?? {},
    })
    .select()
    .single();

  if (error || !data) {
    return NextResponse.json({ error: error?.message ?? "Insert failed" }, { status: 500 });
  }

  return NextResponse.json(data, { status: 201 });
}
