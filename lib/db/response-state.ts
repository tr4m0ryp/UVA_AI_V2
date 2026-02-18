import { createServerClient } from "./client";
import type { OpenAIMessage } from "../uva/translator";

export interface ResponseState {
  respId: string;
  model: string;
  messages: OpenAIMessage[];
  apiKeyId: string;
}

export async function storeResponseState(state: ResponseState): Promise<void> {
  const db = createServerClient();
  await db.from("response_state").upsert({
    id: state.respId,
    api_key_id: state.apiKeyId,
    model: state.model,
    messages_json: state.messages,
  });
}

export async function lookupResponseState(respId: string): Promise<ResponseState | null> {
  const db = createServerClient();
  const { data, error } = await db
    .from("response_state")
    .select("id, api_key_id, model, messages_json, created_at")
    .eq("id", respId)
    .single();

  if (error || !data) return null;

  // Treat entries older than 2 hours as expired
  const age = Date.now() - new Date(data.created_at as string).getTime();
  if (age > 2 * 60 * 60 * 1000) {
    void db.from("response_state").delete().eq("id", respId);
    return null;
  }

  return {
    respId: data.id as string,
    model: data.model as string,
    messages: data.messages_json as OpenAIMessage[],
    apiKeyId: data.api_key_id as string,
  };
}

export async function deleteResponseState(respId: string): Promise<void> {
  const db = createServerClient();
  await db.from("response_state").delete().eq("id", respId);
}
