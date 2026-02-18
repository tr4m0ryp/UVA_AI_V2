import { NextRequest, NextResponse } from "next/server";
import { createServerClient } from "@/lib/db/client";
import { requireApiAuth } from "@/lib/auth/session";

// PUT /api/auth/session - update the stored UvA session cookie for the current user
export async function PUT(req: NextRequest) {
  const authResult = await requireApiAuth();
  if (authResult.type === "error") return authResult.response;
  const user = authResult.user;

  let body: { uva_session?: string };
  try {
    body = await req.json();
  } catch {
    return NextResponse.json({ error: "Invalid JSON" }, { status: 400 });
  }

  if (typeof body.uva_session !== "string") {
    return NextResponse.json({ error: "uva_session is required" }, { status: 400 });
  }

  const db = createServerClient();
  const { error } = await db
    .from("users")
    .update({ uva_session: body.uva_session.trim() })
    .eq("id", user.id);

  if (error) {
    return NextResponse.json({ error: error.message }, { status: 500 });
  }

  return NextResponse.json({ ok: true });
}
