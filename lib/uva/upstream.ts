import type { UvaRequest } from "./translator";

const UVA_CHAT_URL = "https://aichat.uva.nl/api/v1/chat";

export interface OpenAIDelta {
  id: string;
  object: "chat.completion.chunk";
  created: number;
  model: string;
  choices: {
    index: number;
    delta: { role?: string; content?: string };
    finish_reason: string | null;
  }[];
}

function makeChunk(content: string, model: string, done = false): OpenAIDelta {
  const id = `chatcmpl-${Date.now()}`;
  return {
    id,
    object: "chat.completion.chunk",
    created: Math.floor(Date.now() / 1000),
    model,
    choices: [
      {
        index: 0,
        delta: done ? {} : { content },
        finish_reason: done ? "stop" : null,
      },
    ],
  };
}

// Parses a single UvA SSE data line and returns text delta or null.
function parseUvaLine(data: string): string | null {
  if (data === "[DONE]") return null;

  let parsed: Record<string, unknown>;
  try {
    parsed = JSON.parse(data);
  } catch {
    return null;
  }

  // Vercel AI SDK v2 format (primary)
  if (parsed.type === "text-delta" && typeof parsed.delta === "string") {
    return parsed.delta;
  }

  // OpenAI passthrough format
  if (Array.isArray(parsed.choices)) {
    const choices = parsed.choices as {
      delta?: { content?: string };
    }[];
    if (choices[0]?.delta?.content) {
      return choices[0].delta.content;
    }
  }

  return null;
}

// Canonical SSE reader: fetches the UvA stream and yields raw text deltas.
async function* fetchUvaStream(
  uvaRequest: UvaRequest,
  sessionCookie: string
): AsyncGenerator<string> {
  const response = await fetch(UVA_CHAT_URL, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Cookie: sessionCookie,
      Accept: "text/event-stream",
      "User-Agent":
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36",
      Referer: "https://aichat.uva.nl/",
      Origin: "https://aichat.uva.nl",
    },
    body: JSON.stringify(uvaRequest),
  });

  if (!response.ok) {
    const body = await response.text().catch(() => "");
    throw new Error(
      `Upstream error ${response.status}: ${response.statusText} - ${body}`
    );
  }

  if (!response.body) {
    throw new Error("Upstream returned no body");
  }

  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  let done = false;
  let buffer = "";

  while (!done) {
    const { value, done: streamDone } = await reader.read();
    done = streamDone;

    if (value) {
      buffer += decoder.decode(value, { stream: true });
      const lines = buffer.split("\n");
      buffer = lines.pop() ?? "";

      for (const line of lines) {
        if (line.startsWith("data: ")) {
          const data = line.slice(6).trim();
          if (data === "[DONE]") {
            done = true;
            break;
          }
          const text = parseUvaLine(data);
          if (text !== null && text.length > 0) {
            yield text;
          }
        }
      }
    }
  }

  // Drain remaining buffer
  if (buffer.trim().startsWith("data: ")) {
    const data = buffer.trim().slice(6).trim();
    const text = parseUvaLine(data);
    if (text !== null && text.length > 0) {
      yield text;
    }
  }
}

// Streams UvA chat and yields OpenAI-compatible SSE chunks.
export async function* streamUvaChat(
  uvaRequest: UvaRequest,
  sessionCookie: string,
  openaiModel: string
): AsyncGenerator<string> {
  // Opening role chunk
  const openChunk = makeChunk("", openaiModel, false);
  openChunk.choices[0].delta = { role: "assistant" };
  yield `data: ${JSON.stringify(openChunk)}\n\n`;

  for await (const text of fetchUvaStream(uvaRequest, sessionCookie)) {
    const chunk = makeChunk(text, openaiModel);
    yield `data: ${JSON.stringify(chunk)}\n\n`;
  }

  // Terminal done chunk
  const doneChunk = makeChunk("", openaiModel, true);
  yield `data: ${JSON.stringify(doneChunk)}\n\n`;
  yield "data: [DONE]\n\n";
}

// Yields raw text delta strings (no OpenAI wrapper). Used by /v1/responses.
export async function* streamUvaRawDeltas(
  uvaRequest: UvaRequest,
  sessionCookie: string
): AsyncGenerator<string> {
  yield* fetchUvaStream(uvaRequest, sessionCookie);
}

// Accumulates the full text response. Used for tool-call buffering.
export async function collectUvaText(
  uvaRequest: UvaRequest,
  sessionCookie: string
): Promise<string> {
  let full = "";
  for await (const chunk of fetchUvaStream(uvaRequest, sessionCookie)) {
    full += chunk;
  }
  return full;
}
