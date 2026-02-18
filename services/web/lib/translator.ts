import { v4 as uuidv4 } from "uuid";
import { modelsToUva } from "./models";

export interface OpenAIMessage {
  role: "system" | "user" | "assistant";
  content: string;
}

export interface UvaRequestFlags {
  studyMode: boolean;
  enforceInternetSearch: boolean;
  enforceArtifactCreation: boolean;
  enforceImageGeneration: boolean;
  regenerate: boolean;
  continue: boolean;
  isNewChat: boolean;
}

export interface UvaRequest {
  id: string;
  message: {
    id: string;
    role: string;
    content: string;
    parts: { type: string; text: string }[];
  };
  flags: UvaRequestFlags;
  overrides: {
    model: string;
    personaId: string;
  };
  requestTime: string;
}

const DEFAULT_FLAGS: UvaRequestFlags = {
  studyMode: false,
  enforceInternetSearch: false,
  enforceArtifactCreation: false,
  enforceImageGeneration: false,
  regenerate: false,
  continue: false,
  isNewChat: true,
};

// Build the UvA /api/v1/chat request body from an OpenAI messages array.
// The UvA API expects only the final user message, with any system prompt
// prepended to the content if present.
export function buildUvaRequest(
  messages: OpenAIMessage[],
  model: string,
  flags?: Partial<UvaRequestFlags>
): UvaRequest {
  const uvaModel = modelsToUva(model);

  // Collect system prompt and find last user message
  const systemParts: string[] = [];
  let lastUserContent = "";

  for (const msg of messages) {
    if (msg.role === "system") {
      systemParts.push(msg.content);
    }
  }

  // Last user message
  for (let i = messages.length - 1; i >= 0; i--) {
    if (messages[i].role === "user") {
      lastUserContent = messages[i].content;
      break;
    }
  }

  // Prepend system prompt if present
  const finalContent =
    systemParts.length > 0
      ? `${systemParts.join("\n\n")}\n\n${lastUserContent}`
      : lastUserContent;

  const mergedFlags: UvaRequestFlags = { ...DEFAULT_FLAGS, ...flags };

  return {
    id: uuidv4(),
    message: {
      id: uuidv4(),
      role: "user",
      content: finalContent,
      parts: [{ type: "text", text: finalContent }],
    },
    flags: mergedFlags,
    overrides: {
      model: uvaModel,
      personaId: "",
    },
    requestTime: new Date().toISOString(),
  };
}
