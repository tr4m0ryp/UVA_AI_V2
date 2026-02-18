import type { ParsedToolCall } from "../uva/tools";

// All emit* functions return a Uint8Array ready to enqueue into a ReadableStream controller.
const enc = new TextEncoder();

function sse(event: string, data: unknown): Uint8Array {
  return enc.encode(`event: ${event}\ndata: ${JSON.stringify(data)}\n\n`);
}

// --- Text path ---

export function emitCreated(respId: string, model: string): Uint8Array {
  return sse("response.created", {
    type: "response.created",
    response: {
      id: respId,
      object: "realtime.response",
      status: "in_progress",
      model,
      output: [],
    },
  });
}

export function emitOutputItemAdded(msgId: string, idx: number): Uint8Array {
  return sse("response.output_item.added", {
    type: "response.output_item.added",
    output_index: idx,
    item: { id: msgId, object: "realtime.item", type: "message", role: "assistant", content: [] },
  });
}

export function emitContentPartAdded(msgId: string, idx: number): Uint8Array {
  return sse("response.content_part.added", {
    type: "response.content_part.added",
    item_id: msgId,
    output_index: idx,
    content_index: 0,
    part: { type: "text", text: "" },
  });
}

export function emitTextDelta(msgId: string, delta: string, idx: number): Uint8Array {
  return sse("response.output_text.delta", {
    type: "response.output_text.delta",
    item_id: msgId,
    output_index: idx,
    content_index: 0,
    delta,
  });
}

export function emitTextDone(msgId: string, text: string, idx: number): Uint8Array {
  return sse("response.output_text.done", {
    type: "response.output_text.done",
    item_id: msgId,
    output_index: idx,
    content_index: 0,
    text,
  });
}

export function emitContentPartDone(msgId: string, text: string, idx: number): Uint8Array {
  return sse("response.content_part.done", {
    type: "response.content_part.done",
    item_id: msgId,
    output_index: idx,
    content_index: 0,
    part: { type: "text", text },
  });
}

export function emitOutputItemDone(msgId: string, text: string, idx: number): Uint8Array {
  return sse("response.output_item.done", {
    type: "response.output_item.done",
    output_index: idx,
    item: {
      id: msgId,
      object: "realtime.item",
      type: "message",
      role: "assistant",
      content: [{ type: "output_text", text }],
    },
  });
}

export function emitResponseCompleted(
  respId: string,
  model: string,
  msgId: string,
  text: string
): Uint8Array {
  return sse("response.completed", {
    type: "response.completed",
    response: {
      id: respId,
      object: "realtime.response",
      status: "completed",
      model,
      output: [
        {
          id: msgId,
          object: "realtime.item",
          type: "message",
          role: "assistant",
          content: [{ type: "output_text", text }],
        },
      ],
    },
  });
}

// --- Function call path ---

export function emitFcItemAdded(
  fcId: string,
  name: string,
  callId: string,
  idx: number
): Uint8Array {
  return sse("response.output_item.added", {
    type: "response.output_item.added",
    output_index: idx,
    item: {
      id: fcId,
      object: "realtime.item",
      type: "function_call",
      status: "in_progress",
      name,
      call_id: callId,
      arguments: "",
    },
  });
}

export function emitFcArgsDelta(fcId: string, delta: string, idx: number): Uint8Array {
  return sse("response.function_call_arguments.delta", {
    type: "response.function_call_arguments.delta",
    item_id: fcId,
    output_index: idx,
    delta,
  });
}

export function emitFcArgsDone(fcId: string, args: string, idx: number): Uint8Array {
  return sse("response.function_call_arguments.done", {
    type: "response.function_call_arguments.done",
    item_id: fcId,
    output_index: idx,
    arguments: args,
  });
}

export function emitFcItemDone(
  fcId: string,
  name: string,
  callId: string,
  args: string,
  idx: number
): Uint8Array {
  return sse("response.output_item.done", {
    type: "response.output_item.done",
    output_index: idx,
    item: {
      id: fcId,
      object: "realtime.item",
      type: "function_call",
      status: "completed",
      name,
      call_id: callId,
      arguments: args,
    },
  });
}

export function emitFcResponseCompleted(
  respId: string,
  model: string,
  calls: ParsedToolCall[]
): Uint8Array {
  return sse("response.completed", {
    type: "response.completed",
    response: {
      id: respId,
      object: "realtime.response",
      status: "completed",
      model,
      output: calls.map((c, i) => ({
        id: `fc_${i}`,
        object: "realtime.item",
        type: "function_call",
        status: "completed",
        name: c.name,
        call_id: c.callId,
        arguments: c.args,
      })),
    },
  });
}

// --- Non-streaming JSON builders ---

export function buildTextResponseJson(
  respId: string,
  model: string,
  msgId: string,
  text: string
): object {
  return {
    id: respId,
    object: "response",
    status: "completed",
    model,
    output: [
      {
        id: msgId,
        object: "realtime.item",
        type: "message",
        role: "assistant",
        content: [{ type: "output_text", text }],
      },
    ],
  };
}

export function buildFcResponseJson(
  respId: string,
  model: string,
  calls: ParsedToolCall[]
): object {
  return {
    id: respId,
    object: "response",
    status: "completed",
    model,
    output: calls.map((c, i) => ({
      id: `fc_${i}`,
      object: "realtime.item",
      type: "function_call",
      status: "completed",
      name: c.name,
      call_id: c.callId,
      arguments: c.args,
    })),
  };
}
