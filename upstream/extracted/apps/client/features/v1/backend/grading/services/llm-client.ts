const DEFAULT_PROXY_URL = "http://localhost:8787"

interface LLMMessage {
	role: "system" | "user" | "assistant"
	content: string
}

interface LLMRequestOptions {
	model: string
	messages: LLMMessage[]
	temperature?: number
	max_tokens?: number
}

interface LLMChoice {
	message: { role: string; content: string }
	finish_reason: string
}

interface LLMResponse {
	choices: LLMChoice[]
}

function getProxyUrl(): string {
	return process.env.GRADING_LLM_PROXY_URL || DEFAULT_PROXY_URL
}

export async function callLLM(options: LLMRequestOptions): Promise<string> {
	const { model, messages, temperature = 0.3, max_tokens = 4096 } = options
	const url = `${getProxyUrl()}/v1/chat/completions`

	const body = JSON.stringify({
		model,
		messages,
		temperature,
		max_tokens,
		stream: false,
	})

	let lastError: Error | null = null

	for (let attempt = 0; attempt < 2; attempt++) {
		try {
			const response = await fetch(url, {
				method: "POST",
				headers: { "Content-Type": "application/json" },
				body,
			})

			if (response.status >= 500 && attempt === 0) {
				lastError = new Error(`LLM proxy returned ${response.status}`)
				continue
			}

			if (!response.ok) {
				const text = await response.text()
				throw new Error(`LLM request failed (${response.status}): ${text}`)
			}

			const data = (await response.json()) as LLMResponse
			const content = data.choices?.[0]?.message?.content

			if (!content) {
				throw new Error("LLM returned empty response")
			}

			return content
		} catch (error) {
			if (attempt === 0 && lastError) continue
			throw error
		}
	}

	throw lastError || new Error("LLM request failed after retries")
}

export function extractJSON<T>(raw: string): T {
	const jsonMatch = raw.match(/```json\s*([\s\S]*?)```/)
	const text = jsonMatch ? jsonMatch[1].trim() : raw.trim()

	const start = text.indexOf("{")
	const end = text.lastIndexOf("}")
	if (start === -1 || end === -1) {
		throw new Error("No JSON object found in LLM response")
	}

	return JSON.parse(text.slice(start, end + 1)) as T
}
