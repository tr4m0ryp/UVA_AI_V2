import type {
	AnchorData,
	AnchorSubmission,
	LLMGradingResponse,
	StructuredRubric,
} from "../models/grading-types"
import { callLLM, extractJSON } from "./llm-client"

export interface SupabaseClient {
	from(table: string): {
		update(data: Record<string, unknown>): { eq(col: string, val: string): Promise<{ error: unknown }> }
		select(cols?: string): { eq(col: string, val: string): Promise<{ data: unknown[]; error: unknown }> }
	}
}

function buildGradingSystemPrompt(rubric: StructuredRubric): string {
	const criteriaDesc = rubric.criteria
		.map((c) => `- ${c.name} (max ${c.max_points} points)`)
		.join("\n")

	return `You are a consistent, fair academic grader. Grade the student submission against the rubric.

Rubric criteria:
${criteriaDesc}

Total maximum: ${rubric.total_max} points
Pass threshold: ${rubric.pass_threshold} points

Output ONLY valid JSON:
{
  "scores": [{"criterion_name": "...", "score": 8, "max_points": 10}],
  "total_score": 72,
  "feedback": "Constructive feedback for the student in English",
  "improvement_comments": "Specific suggestions for improvement in English",
  "reasoning": "Internal grading rationale explaining score decisions"
}

Rules:
- Score each criterion independently based on the rubric levels
- total_score must equal the sum of individual scores
- Be consistent: similar quality work should receive similar scores
- Feedback must be constructive and in English
- Do not be lenient or harsh -- apply the rubric as written`
}

function buildGradingUserPrompt(
	modelAnswer: string,
	submission: string,
	anchors?: AnchorData,
): string {
	const parts: string[] = []

	if (modelAnswer) {
		parts.push("MODEL ANSWER (reference):", modelAnswer.slice(0, 3000), "")
	}

	if (anchors) {
		parts.push("ANCHOR SUBMISSIONS (for calibration):")
		for (const [level, anchor] of Object.entries(anchors)) {
			const a = anchor as AnchorSubmission
			parts.push(
				`${level.toUpperCase()} anchor (score: ${a.total_score}):`,
				a.content_excerpt.slice(0, 500),
				"",
			)
		}
	}

	parts.push("STUDENT SUBMISSION TO GRADE:", submission)
	return parts.join("\n")
}

export async function gradeSubmission(
	model: string,
	rubric: StructuredRubric,
	modelAnswer: string,
	submissionContent: string,
	anchors?: AnchorData,
): Promise<LLMGradingResponse> {
	const systemPrompt = buildGradingSystemPrompt(rubric)
	const userPrompt = buildGradingUserPrompt(modelAnswer, submissionContent, anchors)

	const response = await callLLM({
		model,
		messages: [
			{ role: "system", content: systemPrompt },
			{ role: "user", content: userPrompt },
		],
		temperature: 0.3,
		max_tokens: 2048,
	})

	return extractJSON<LLMGradingResponse>(response)
}

export async function updateSubmissionRecord(
	supabase: SupabaseClient,
	submissionId: string,
	result: LLMGradingResponse,
	status: string,
): Promise<void> {
	await supabase.from("submissions").update({
		scores: result.scores,
		total_score: result.total_score,
		feedback: result.feedback,
		improvement_comments: result.improvement_comments,
		reasoning: result.reasoning,
		status,
		graded_at: new Date().toISOString(),
	}).eq("id", submissionId)
}

export async function incrementGradedCount(
	supabase: SupabaseClient,
	sessionId: string,
): Promise<void> {
	const { data } = await supabase
		.from("grading_sessions")
		.select("graded_count")
		.eq("id", sessionId) as { data: Array<{ graded_count: number }>; error: unknown }

	if (data?.[0]) {
		await supabase.from("grading_sessions").update({
			graded_count: data[0].graded_count + 1,
		}).eq("id", sessionId)
	}
}
