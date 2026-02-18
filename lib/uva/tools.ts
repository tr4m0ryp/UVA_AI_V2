import { v4 as uuidv4 } from "uuid";
import type { OpenAIMessage } from "./translator";

export interface ParsedToolCall {
  name: string;
  args: string;
  callId: string;
}

// Converts an OpenAI tools array to an XML system prompt string.
// The model reads this to know which tools are available and how to call them.
export function toolDefinitionsToSysprompt(tools: unknown[]): string {
  const defs = tools
    .map((t) => {
      const tool = t as Record<string, unknown>;
      // Support both flat ({name, description, parameters}) and nested ({type:"function", function:{...}})
      const fn =
        tool.type === "function" && tool.function
          ? (tool.function as Record<string, unknown>)
          : tool;
      const name = fn.name as string;
      const description = (fn.description as string) ?? "";
      const params = fn.parameters ?? {};
      return (
        `<tool name="${name}">\n` +
        `  <description>${description}</description>\n` +
        `  <parameters>${JSON.stringify(params)}</parameters>\n` +
        `</tool>`
      );
    })
    .join("\n");

  return (
    "You have access to the following tools. " +
    "To call a tool, emit exactly one XML block with this format:\n" +
    '<tool_call name="TOOL_NAME">{"arg1": "value1"}</tool_call>\n\n' +
    "Available tools:\n" +
    defs +
    "\n\nIf you do not need to call a tool, reply normally without any XML blocks."
  );
}

// Prepend a system tool prompt to the messages array.
export function injectToolSysprompt(
  messages: OpenAIMessage[],
  sysprompt: string
): OpenAIMessage[] {
  const existing = messages.findIndex((m) => m.role === "system");
  if (existing !== -1) {
    const copy = [...messages];
    copy[existing] = {
      ...copy[existing],
      content: copy[existing].content + "\n\n" + sysprompt,
    };
    return copy;
  }
  return [{ role: "system", content: sysprompt }, ...messages];
}

// Convert {role:"tool"} messages to {role:"user"} messages for UvA.
// UvA does not accept "tool" role messages; fold results into user turns.
export function foldToolResults(messages: OpenAIMessage[]): OpenAIMessage[] {
  return messages.map((m) => {
    const msg = m as { role: string; content: string; tool_call_id?: string };
    if (msg.role !== "tool") return m;
    return {
      role: "user" as const,
      content: `[Tool result for ${msg.tool_call_id ?? "call"}]: ${msg.content}`,
    };
  });
}

// Scan text for <tool_call name="X">JSON</tool_call> blocks.
export function parseToolCallsFromText(text: string): ParsedToolCall[] {
  const calls: ParsedToolCall[] = [];
  const re = /<tool_call\s+name="([^"]+)">([\s\S]*?)<\/tool_call>/g;
  let match: RegExpExecArray | null;
  while ((match = re.exec(text)) !== null) {
    calls.push({
      name: match[1],
      args: match[2].trim(),
      callId: `call_${uuidv4().replace(/-/g, "").slice(0, 24)}`,
    });
  }
  return calls;
}
