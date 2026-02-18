import type { StructuredRubric } from "../models/grading-types"
import { callLLM, extractJSON } from "./llm-client"

const RUBRIC_SYSTEM_PROMPT = `You are a rubric parser. Convert the provided grading rubric into a structured JSON format.

Output ONLY valid JSON with this exact structure:
{
  "criteria": [
    {
      "name": "Criterion Name",
      "max_points": 30,
      "levels": [
        {"range": [25, 30], "label": "Excellent", "description": "Description of this level"},
        {"range": [18, 24], "label": "Good", "description": "Description of this level"},
        {"range": [10, 17], "label": "Sufficient", "description": "Description of this level"},
        {"range": [0, 9], "label": "Insufficient", "description": "Description of this level"}
      ]
    }
  ],
  "total_max": 100,
  "pass_threshold": 55
}

Rules:
- Each criterion must have a name, max_points, and at least 2 levels
- Level ranges must be non-overlapping and cover 0 to max_points
- total_max must equal the sum of all criteria max_points
- If no pass threshold is specified, default to 55% of total_max
- Preserve the rubric's original criteria names and descriptions`

function validateRubric(rubric: StructuredRubric): void {
	if (!rubric.criteria || rubric.criteria.length === 0) {
		throw new Error("Rubric must have at least one criterion")
	}

	const criteriaSum = rubric.criteria.reduce((sum, c) => sum + c.max_points, 0)

	if (criteriaSum !== rubric.total_max) {
		throw new Error(
			`total_max (${rubric.total_max}) does not match sum of criteria (${criteriaSum})`
		)
	}

	for (const criterion of rubric.criteria) {
		if (!criterion.name || criterion.max_points <= 0) {
			throw new Error(`Invalid criterion: ${criterion.name || "unnamed"}`)
		}

		if (!criterion.levels || criterion.levels.length < 2) {
			throw new Error(`Criterion "${criterion.name}" must have at least 2 levels`)
		}

		for (const level of criterion.levels) {
			if (
				!Array.isArray(level.range) ||
				level.range.length !== 2 ||
				level.range[0] > level.range[1]
			) {
				throw new Error(
					`Invalid range in criterion "${criterion.name}", level "${level.label}"`
				)
			}
		}
	}

	if (rubric.pass_threshold < 0 || rubric.pass_threshold > rubric.total_max) {
		throw new Error("pass_threshold must be between 0 and total_max")
	}
}

export async function parseRubric(rawRubric: string): Promise<StructuredRubric> {
	const response = await callLLM({
		model: "gpt-5.1",
		messages: [
			{ role: "system", content: RUBRIC_SYSTEM_PROMPT },
			{ role: "user", content: rawRubric },
		],
		temperature: 0.7,
		max_tokens: 4096,
	})

	const rubric = extractJSON<StructuredRubric>(response)
	validateRubric(rubric)
	return rubric
}
