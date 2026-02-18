export interface ModelInfo {
  id: string;
  uvaId: string;
  name: string;
  contextWindow: number;
  maxOutput: number;
}

const MODEL_MAP: ModelInfo[] = [
  {
    id: "gpt-5",
    uvaId: "gpt-5",
    name: "GPT-5",
    contextWindow: 128000,
    maxOutput: 16384,
  },
  {
    id: "gpt-5.1",
    uvaId: "gpt-5.1",
    name: "GPT-5.1",
    contextWindow: 128000,
    maxOutput: 16384,
  },
  {
    id: "gpt-5-mini",
    uvaId: "gpt-5-mini",
    name: "GPT-5 Mini",
    contextWindow: 128000,
    maxOutput: 16384,
  },
  {
    id: "gpt-5-nano",
    uvaId: "gpt-5-nano",
    name: "GPT-5 Nano",
    contextWindow: 128000,
    maxOutput: 8192,
  },
  {
    id: "gpt-4.1",
    uvaId: "gpt-4.1",
    name: "GPT-4.1",
    contextWindow: 128000,
    maxOutput: 16384,
  },
  {
    id: "gpt-4o",
    uvaId: "gpt-4o",
    name: "GPT-4o",
    contextWindow: 128000,
    maxOutput: 4096,
  },
  {
    id: "gpt-oss-120b",
    uvaId: "gpt-oss-120b",
    name: "GPT OSS 120B",
    contextWindow: 128000,
    maxOutput: 4096,
  },
];

export function modelsToUva(openaiId: string): string {
  const model = MODEL_MAP.find((m) => m.id === openaiId);
  return model ? model.uvaId : openaiId;
}

export function modelsToOpenAI(uvaId: string): string {
  const model = MODEL_MAP.find((m) => m.uvaId === uvaId);
  return model ? model.id : uvaId;
}

export function listModels(): object {
  const now = Math.floor(Date.now() / 1000);
  return {
    object: "list",
    data: MODEL_MAP.map((m) => ({
      id: m.id,
      object: "model",
      created: now,
      owned_by: "uva",
      context_window: m.contextWindow,
      max_output: m.maxOutput,
    })),
  };
}

export function isValidModel(id: string): boolean {
  return MODEL_MAP.some((m) => m.id === id || m.uvaId === id);
}
