import { NextResponse } from "next/server";
import {
  buildUvaRequest,
  type OpenAIMessage,
} from "../uva/translator";
import { streamUvaRawDeltas, collectUvaText } from "../uva/upstream";
import {
  toolDefinitionsToSysprompt,
  injectToolSysprompt,
  foldToolResults,
  parseToolCallsFromText,
  type ParsedToolCall,
} from "../uva/tools";
import {
  emitCreated,
  emitOutputItemAdded,
  emitContentPartAdded,
  emitTextDelta,
  emitTextDone,
  emitContentPartDone,
  emitOutputItemDone,
  emitResponseCompleted,
  emitFcItemAdded,
  emitFcArgsDelta,
  emitFcArgsDone,
  emitFcItemDone,
  emitFcResponseCompleted,
  buildTextResponseJson,
  buildFcResponseJson,
} from "./emitter";
import {
  storeResponseState,
  deleteResponseState,
} from "../db/response-state";

const SSE_HEADERS = {
  "Content-Type": "text/event-stream",
  "Cache-Control": "no-cache",
  Connection: "keep-alive",
};

function makeFcStream(
  respId: string,
  model: string,
  calls: ParsedToolCall[]
): ReadableStream {
  return new ReadableStream({
    start(controller) {
      controller.enqueue(emitCreated(respId, model));
      calls.forEach((call, i) => {
        const fcId = `fc_${i}`;
        controller.enqueue(emitFcItemAdded(fcId, call.name, call.callId, i));
        controller.enqueue(emitFcArgsDelta(fcId, call.args, i));
        controller.enqueue(emitFcArgsDone(fcId, call.args, i));
        controller.enqueue(emitFcItemDone(fcId, call.name, call.callId, call.args, i));
      });
      controller.enqueue(emitFcResponseCompleted(respId, model, calls));
      controller.close();
    },
  });
}

function makeTextStream(
  respId: string,
  model: string,
  msgId: string,
  text: string
): ReadableStream {
  return new ReadableStream({
    start(controller) {
      controller.enqueue(emitCreated(respId, model));
      controller.enqueue(emitOutputItemAdded(msgId, 0));
      controller.enqueue(emitContentPartAdded(msgId, 0));
      controller.enqueue(emitTextDelta(msgId, text, 0));
      controller.enqueue(emitTextDone(msgId, text, 0));
      controller.enqueue(emitContentPartDone(msgId, text, 0));
      controller.enqueue(emitOutputItemDone(msgId, text, 0));
      controller.enqueue(emitResponseCompleted(respId, model, msgId, text));
      controller.close();
    },
  });
}

export interface HandlerParams {
  messages: OpenAIMessage[];
  model: string;
  tools: unknown[] | null;
  wantStream: boolean;
  uvaSession: string;
  respId: string;
  msgId: string;
  apiKeyId: string;
  prevRespId: string | undefined;
  logRequest: (inputChars: number, outputChars: number, ok: boolean) => void;
}

export async function handleToolPath(p: HandlerParams): Promise<Response> {
  const { messages, model, tools, wantStream, uvaSession, respId, msgId, apiKeyId, prevRespId, logRequest } = p;

  const sysprompt = toolDefinitionsToSysprompt(tools!);
  let augmented = injectToolSysprompt(messages, sysprompt);
  augmented = foldToolResults(augmented);
  const uvaRequest = buildUvaRequest(augmented, model);

  const fullText = await collectUvaText(uvaRequest, uvaSession);
  const calls = parseToolCallsFromText(fullText);
  const inputChars = messages.reduce((s, m) => s + m.content.length, 0);

  if (calls.length > 0) {
    const assistantTurn: OpenAIMessage = { role: "assistant", content: fullText };
    await storeResponseState({ respId, model, messages: [...messages, assistantTurn], apiKeyId });
    if (prevRespId) await deleteResponseState(prevRespId);
    logRequest(inputChars, fullText.length, true);

    if (!wantStream) return NextResponse.json(buildFcResponseJson(respId, model, calls));
    return new Response(makeFcStream(respId, model, calls), { headers: SSE_HEADERS });
  }

  // No calls found — text fallback
  const assistantTurn: OpenAIMessage = { role: "assistant", content: fullText };
  await storeResponseState({ respId, model, messages: [...messages, assistantTurn], apiKeyId });
  if (prevRespId) await deleteResponseState(prevRespId);
  logRequest(inputChars, fullText.length, true);

  if (!wantStream) return NextResponse.json(buildTextResponseJson(respId, model, msgId, fullText));
  return new Response(makeTextStream(respId, model, msgId, fullText), { headers: SSE_HEADERS });
}

export async function handleTextPath(p: HandlerParams): Promise<Response> {
  const { messages, model, wantStream, uvaSession, respId, msgId, apiKeyId, prevRespId, logRequest } = p;
  const uvaRequest = buildUvaRequest(messages, model);
  const inputChars = messages.reduce((s, m) => s + m.content.length, 0);

  if (!wantStream) {
    const fullText = await collectUvaText(uvaRequest, uvaSession);
    const assistantTurn: OpenAIMessage = { role: "assistant", content: fullText };
    await storeResponseState({ respId, model, messages: [...messages, assistantTurn], apiKeyId });
    if (prevRespId) await deleteResponseState(prevRespId);
    logRequest(inputChars, fullText.length, true);
    return NextResponse.json(buildTextResponseJson(respId, model, msgId, fullText));
  }

  let accumulated = "";
  let outputChars = 0;
  let success = true;

  const stream = new ReadableStream({
    async start(controller) {
      controller.enqueue(emitCreated(respId, model));
      controller.enqueue(emitOutputItemAdded(msgId, 0));
      controller.enqueue(emitContentPartAdded(msgId, 0));

      try {
        for await (const delta of streamUvaRawDeltas(uvaRequest, uvaSession)) {
          accumulated += delta;
          outputChars += delta.length;
          controller.enqueue(emitTextDelta(msgId, delta, 0));
        }
      } catch (err) {
        success = false;
        const enc = new TextEncoder();
        controller.enqueue(enc.encode(`event: error\ndata: {"error":"${String(err)}"}\n\n`));
      } finally {
        controller.enqueue(emitTextDone(msgId, accumulated, 0));
        controller.enqueue(emitContentPartDone(msgId, accumulated, 0));
        controller.enqueue(emitOutputItemDone(msgId, accumulated, 0));
        controller.enqueue(emitResponseCompleted(respId, model, msgId, accumulated));
        controller.close();

        const assistantTurn: OpenAIMessage = { role: "assistant", content: accumulated };
        void storeResponseState({ respId, model, messages: [...messages, assistantTurn], apiKeyId })
          .then(() => { if (prevRespId) return deleteResponseState(prevRespId); })
          .catch(() => {});
        logRequest(inputChars, outputChars, success);
      }
    },
  });

  return new Response(stream, { headers: SSE_HEADERS });
}
