import { NextRequest, NextResponse } from "next/server";
import { resolveBearerKey, logApiRequest } from "@/lib/auth/api-key";
import { buildUvaRequest } from "@/lib/uva/translator";
import { streamUvaChat } from "@/lib/uva/upstream";
import { isValidModel } from "@/lib/uva/models";

export async function POST(req: NextRequest) {
  const authResult = await resolveBearerKey(req);
  if (!authResult.ok) return authResult.response;
  const { apiKey, uvaSession, db } = authResult.data;

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

  const model = body.model ?? (apiKey.model as string);
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
        logApiRequest(db, apiKey.id as string, model, startTime, inputChars, outputChars, success);
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
