import { callLLM, extractJSON } from "./llm-client"

interface ModelSelection {
	model: string
	reasoning: string
}

const MODEL_SELECTION_PROMPT = `You are a model selection assistant. Based on the assignment description and rubric summary, recommend the best AI model for grading.

Available models and their strengths:
- "claude-sonnet-4.5": Best for essays, humanities, nuanced text analysis, creative writing evaluation
- "o3": Best for STEM subjects, mathematics, logic problems, formal proofs, quantitative analysis
- "gpt-5.1": Best for code review, structured evaluation, technical reports, mixed-format assignments

Consider:
1. The subject domain (humanities vs STEM vs technical)
2. Whether grading requires nuanced interpretation vs strict correctness
3. The type of rubric criteria (qualitative vs quantitative)

Output ONLY valid JSON:
{
  "model": "model-name-here",
  "reasoning": "Brief explanation of why this model is best suited"
}`

export async function selectModel(
	assignmentText: string,
	rubricSummary: string,
): Promise<ModelSelection> {
	const userPrompt = [
		"Assignment:",
		assignmentText.slice(0, 2000),
		"",
		"Rubric Summary:",
		rubricSummary.slice(0, 1000),
	].join("\n")

	const response = await callLLM({
		model: "gpt-5.1",
		messages: [
			{ role: "system", content: MODEL_SELECTION_PROMPT },
			{ role: "user", content: userPrompt },
		],
		temperature: 0.7,
		max_tokens: 512,
	})

	const result = extractJSON<ModelSelection>(response)

	const validModels = ["claude-sonnet-4.5", "o3", "gpt-5.1"]
	if (!validModels.includes(result.model)) {
		return { model: "gpt-5.1", reasoning: "Default fallback: model selector returned unknown model" }
	}

	return result
}
