import { NextRequest, NextResponse } from "next/server";
import { createServerClient } from "@/lib/supabase";
import { buildUvaRequest } from "@/lib/translator";
import { streamUvaChat } from "@/lib/upstream";
import { isValidModel } from "@/lib/models";
import crypto from "crypto";

function hashKey(key: string): string {
  return crypto.createHash("sha256").update(key).digest("hex");
}

export async function POST(req: NextRequest) {
  const auth = req.headers.get("authorization") ?? "";
  const bearerMatch = auth.match(/^Bearer\s+(.+)$/i);
  if (!bearerMatch) {
    return NextResponse.json({ error: "Missing Bearer token" }, { status: 401 });
  }
  const rawKey = bearerMatch[1].trim();
  const keyHash = hashKey(rawKey);

  const db = createServerClient();

  // Resolve the API key
  const { data: apiKey, error: keyErr } = await db
    .from("api_keys")
    .select("*, users(uva_session)")
    .eq("key_hash", keyHash)
    .eq("is_active", true)
    .single();

  if (keyErr || !apiKey) {
    return NextResponse.json({ error: "Invalid or inactive API key" }, { status: 401 });
  }

  const uvaSession = (apiKey.users as { uva_session: string } | null)?.uva_session ?? "";
  if (!uvaSession) {
    return NextResponse.json(
      { error: "UvA session cookie not configured. Visit /dashboard/settings." },
      { status: 503 }
    );
  }

  let body: {
    model?: string;
    messages?: { role: string; content: string }[];
    stream?: boolean;
    temperature?: number;
    max_tokens?: number;
  };

  try {
    body = await req.json();
  } catch {
    return NextResponse.json({ error: "Invalid JSON body" }, { status: 400 });
  }

  const model = body.model ?? apiKey.model;
  if (!isValidModel(model)) {
    return NextResponse.json({ error: `Unknown model: ${model}` }, { status: 400 });
  }

  const messages = body.messages;
  if (!Array.isArray(messages) || messages.length === 0) {
    return NextResponse.json({ error: "messages array is required" }, { status: 400 });
  }

  const uvaRequest = buildUvaRequest(
    messages as { role: "system" | "user" | "assistant"; content: string }[],
    model
  );

  const startTime = Date.now();
  const inputChars = messages.reduce((sum, m) => sum + (m.content?.length ?? 0), 0);
  let outputChars = 0;
  let success = true;

  const stream = new ReadableStream({
    async start(controller) {
      const encoder = new TextEncoder();
      try {
        for await (const chunk of streamUvaChat(uvaRequest, uvaSession, model)) {
          outputChars += chunk.length;
          controller.enqueue(encoder.encode(chunk));
        }
      } catch (err) {
        success = false;
        const errChunk = `data: {"error":"${String(err)}"}\n\n`;
        controller.enqueue(encoder.encode(errChunk));
      } finally {
        controller.close();
        const latencyMs = Date.now() - startTime;

        // Log the request (fire-and-forget)
        void Promise.resolve(
          db.from("request_logs").insert({
            api_key_id: apiKey.id,
            model,
            input_chars: inputChars,
            output_chars: outputChars,
            success,
            latency_ms: latencyMs,
          })
        )
          .then(() =>
            Promise.resolve(
              db
                .from("api_keys")
                .update({ last_used: new Date().toISOString() })
                .eq("id", apiKey.id)
            )
          )
          .catch(() => {});
      }
    },
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
    },
  });
}
