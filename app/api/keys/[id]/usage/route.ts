import { NextRequest, NextResponse } from "next/server";
import { createServerClient } from "@/lib/db/client";
import { requireApiAuth } from "@/lib/auth/session";

type Params = { params: Promise<{ id: string }> };

export async function GET(_req: NextRequest, { params }: Params) {
  const authResult = await requireApiAuth();
  if (authResult.type === "error") return authResult.response;
  const user = authResult.user;
  const { id } = await params;

  const db = createServerClient();

  // Verify ownership
  const { data: keyRow, error: keyErr } = await db
    .from("api_keys")
    .select("id")
    .eq("id", id)
    .eq("user_id", user.id)
    .single();

  if (keyErr || !keyRow) {
    return NextResponse.json({ error: "Not found" }, { status: 404 });
  }

  // Aggregate stats
  const { data: logs, error: logsErr } = await db
    .from("request_logs")
    .select("model,input_chars,output_chars,success,latency_ms,created_at")
    .eq("api_key_id", id)
    .order("created_at", { ascending: false })
    .limit(500);

  if (logsErr) {
    return NextResponse.json({ error: logsErr.message }, { status: 500 });
  }

  const total_requests = logs?.length ?? 0;
  const total_input_chars = logs?.reduce((s, l) => s + (l.input_chars ?? 0), 0) ?? 0;
  const total_output_chars = logs?.reduce((s, l) => s + (l.output_chars ?? 0), 0) ?? 0;
  const success_count = logs?.filter((l) => l.success).length ?? 0;
  const avg_latency_ms =
    total_requests > 0
      ? Math.round(
          (logs?.reduce((s, l) => s + (l.latency_ms ?? 0), 0) ?? 0) / total_requests
        )
      : 0;

  // Daily breakdown (last 30 days)
  const daily: Record<string, { requests: number; output_chars: number }> = {};
  for (const log of logs ?? []) {
    const day = log.created_at.slice(0, 10);
    if (!daily[day]) daily[day] = { requests: 0, output_chars: 0 };
    daily[day].requests++;
    daily[day].output_chars += log.output_chars ?? 0;
  }

  return NextResponse.json({
    total_requests,
    total_input_chars,
    total_output_chars,
    success_count,
    avg_latency_ms,
    daily,
    recent: logs?.slice(0, 20),
  });
}
