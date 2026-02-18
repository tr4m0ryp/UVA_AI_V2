import { NextRequest, NextResponse } from "next/server";
import crypto from "crypto";
import { resolveBearerKey, logApiRequest } from "@/lib/auth/api-key";
import { messagesFromInput } from "@/lib/uva/translator";
import { isValidModel } from "@/lib/uva/models";
import { lookupResponseState } from "@/lib/db/response-state";
import { handleToolPath, handleTextPath } from "@/lib/responses/handler";

function newRespId(): string {
  return "resp_" + crypto.randomBytes(8).toString("hex");
}

function newMsgId(): string {
  return "msg_" + crypto.randomBytes(6).toString("hex");
}

interface RequestBody {
  model?: string;
  input?: string | unknown[];
  instructions?: string;
  tools?: unknown[];
  stream?: boolean;
  previous_response_id?: string;
}

export async function POST(req: NextRequest) {
  const authResult = await resolveBearerKey(req);
  if (!authResult.ok) return authResult.response;
  const { apiKey, uvaSession, db } = authResult.data;

  let body: RequestBody;
  try {
    body = await req.json();
  } catch {
    return NextResponse.json({ error: "Invalid JSON body" }, { status: 400 });
  }

  const model = body.model ?? (apiKey.model as string);
  if (!isValidModel(model)) {
    return NextResponse.json({ error: `Unknown model: ${model}` }, { status: 400 });
  }

  const wantStream = body.stream !== false;
  const tools = Array.isArray(body.tools) && body.tools.length > 0 ? body.tools : null;
  const input = body.input ?? "";
  const instructions = body.instructions;
  const prevRespId = body.previous_response_id;

  let messages: ReturnType<typeof messagesFromInput>;

  if (prevRespId) {
    const prior = await lookupResponseState(prevRespId);
    if (!prior) {
      return NextResponse.json(
        { error: `previous_response_id not found or expired: ${prevRespId}` },
        { status: 404 }
      );
    }
    const newTurn = messagesFromInput(input as Parameters<typeof messagesFromInput>[0]);
    messages = [...prior.messages, ...newTurn];
  } else {
    messages = messagesFromInput(
      input as Parameters<typeof messagesFromInput>[0],
      instructions
    );
  }

  const respId = newRespId();
  const msgId = newMsgId();
  const startTime = Date.now();

  function logRequest(inputChars: number, outChars: number, ok: boolean) {
    logApiRequest(db, apiKey.id as string, model, startTime, inputChars, outChars, ok);
  }

  const handlerParams = {
    messages,
    model,
    tools,
    wantStream,
    uvaSession,
    respId,
    msgId,
    apiKeyId: apiKey.id as string,
    prevRespId,
    logRequest,
  };

  try {
    if (tools) {
      return await handleToolPath(handlerParams);
    }
    return await handleTextPath(handlerParams);
  } catch (err) {
    return NextResponse.json(
      { error: `Upstream error: ${String(err)}` },
      { status: 502 }
    );
  }
}
