import { NextRequest, NextResponse } from "next/server";
import { createServerClient } from "@/lib/db/client";
import { requireApiAuth } from "@/lib/auth/session";

type Params = { params: Promise<{ id: string }> };

export async function POST(_req: NextRequest, { params }: Params) {
  const authResult = await requireApiAuth();
  if (authResult.type === "error") return authResult.response;
  const user = authResult.user;
  const { id } = await params;

  const db = createServerClient();

  const { data: existing, error: fetchErr } = await db
    .from("api_keys")
    .select("is_active")
    .eq("id", id)
    .eq("user_id", user.id)
    .single();

  if (fetchErr || !existing) {
    return NextResponse.json({ error: "Not found" }, { status: 404 });
  }

  const { data, error } = await db
    .from("api_keys")
    .update({ is_active: !existing.is_active })
    .eq("id", id)
    .eq("user_id", user.id)
    .select("id,is_active")
    .single();

  if (error || !data) {
    return NextResponse.json({ error: error?.message ?? "Update failed" }, { status: 500 });
  }

  return NextResponse.json(data);
}
