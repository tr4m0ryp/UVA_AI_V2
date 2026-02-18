import type {
	AnchorData,
	AnchorSubmission,
	GradingSession,
	LLMGradingResponse,
	SessionStats,
	StructuredRubric,
	Submission,
} from "../models/grading-types"
import {
	type SupabaseClient,
	gradeSubmission,
	updateSubmissionRecord,
	incrementGradedCount,
} from "./grading-helpers"

// Phase 1: Pick random submissions, grade them, select anchors
async function establishAnchors(
	supabase: SupabaseClient,
	session: GradingSession,
	submissions: Submission[],
	rubric: StructuredRubric,
): Promise<AnchorData> {
	const sampleSize = Math.min(8, submissions.length)
	const shuffled = [...submissions].sort(() => Math.random() - 0.5)
	const sample = shuffled.slice(0, sampleSize)

	const results: Array<{ submission: Submission; result: LLMGradingResponse }> = []

	for (const sub of sample) {
		const result = await gradeSubmission(
			session.selectedModel || "gpt-5.1",
			rubric,
			session.modelAnswer,
			sub.contentExtracted || "",
		)
		await updateSubmissionRecord(supabase, sub.id, result, "completed")
		await incrementGradedCount(supabase, session.id)
		results.push({ submission: sub, result })
	}

	results.sort((a, b) => a.result.total_score - b.result.total_score)

	const lowIdx = 0
	const midIdx = Math.floor(results.length / 2)
	const highIdx = results.length - 1

	function toAnchor(entry: { submission: Submission; result: LLMGradingResponse }): AnchorSubmission {
		return {
			student_identifier: entry.submission.studentIdentifier,
			content_excerpt: (entry.submission.contentExtracted || "").slice(0, 1000),
			scores: entry.result.scores,
			total_score: entry.result.total_score,
		}
	}

	const anchors: AnchorData = {
		low: toAnchor(results[lowIdx]),
		mid: toAnchor(results[midIdx]),
		high: toAnchor(results[highIdx]),
	}

	const anchorMap: Record<string, string> = {
		[results[lowIdx].submission.id]: "LOW",
		[results[midIdx].submission.id]: "MID",
		[results[highIdx].submission.id]: "HIGH",
	}

	for (const [id, level] of Object.entries(anchorMap)) {
		await supabase.from("submissions").update({
			is_anchor: true,
			anchor_level: level,
		}).eq("id", id)
	}

	await supabase.from("grading_sessions").update({
		anchor_data: anchors,
	}).eq("id", session.id)

	return anchors
}

// Phase 2: Grade remaining submissions in parallel batches
async function bulkGrade(
	supabase: SupabaseClient,
	session: GradingSession,
	submissions: Submission[],
	rubric: StructuredRubric,
	anchors: AnchorData,
): Promise<void> {
	const remaining = submissions.filter((s) => s.status === "pending")
	const batchSize = 20

	for (let i = 0; i < remaining.length; i += batchSize) {
		const batch = remaining.slice(i, i + batchSize)

		const promises = batch.map(async (sub) => {
			try {
				const result = await gradeSubmission(
					session.selectedModel || "gpt-5.1",
					rubric,
					session.modelAnswer,
					sub.contentExtracted || "",
					anchors,
				)
				await updateSubmissionRecord(supabase, sub.id, result, "completed")
				await incrementGradedCount(supabase, session.id)
			} catch {
				await supabase.from("submissions").update({ status: "failed" }).eq("id", sub.id)
			}
		})

		await Promise.allSettled(promises)
	}
}

// Phase 3: Detect and re-grade anomalies
async function detectAndRegrade(
	supabase: SupabaseClient,
	session: GradingSession,
	rubric: StructuredRubric,
	anchors: AnchorData,
): Promise<SessionStats> {
	const { data: allSubs } = await supabase
		.from("submissions").select("*")
		.eq("session_id", session.id) as { data: Submission[]; error: unknown }

	const completed = allSubs.filter((s) => s.status === "completed" && s.totalScore !== null)
	const scores = completed.map((s) => s.totalScore as number)
	const mean = scores.reduce((a, b) => a + b, 0) / scores.length
	const variance = scores.reduce((a, b) => a + (b - mean) ** 2, 0) / scores.length
	const stdDev = Math.sqrt(variance)
	const threshold = rubric.pass_threshold

	const anomalies = completed.filter((s) => {
		const score = s.totalScore as number
		return Math.abs(score - mean) > 2 * stdDev || Math.abs(score - threshold) <= 2
	})

	let regradeCount = 0
	for (const sub of anomalies) {
		try {
			await supabase.from("submissions").update({ status: "re_grading" }).eq("id", sub.id)
			const result = await gradeSubmission(
				session.selectedModel || "gpt-5.1", rubric,
				session.modelAnswer, sub.contentExtracted || "", anchors,
			)
			await updateSubmissionRecord(supabase, sub.id, result, "completed")
			regradeCount++
		} catch {
			// Keep original grade on re-grade failure
		}
	}

	// Recompute final stats
	const { data: finalSubs } = await supabase
		.from("submissions").select("*")
		.eq("session_id", session.id) as { data: Submission[]; error: unknown }

	const fc = finalSubs.filter((s) => s.status === "completed" && s.totalScore !== null)
	const fs = fc.map((s) => s.totalScore as number)
	const fm = fs.reduce((a, b) => a + b, 0) / fs.length
	const fv = fs.reduce((a, b) => a + (b - fm) ** 2, 0) / fs.length
	const fSorted = [...fs].sort((a, b) => a - b)
	const fMedian = fSorted.length % 2 === 0
		? (fSorted[fSorted.length / 2 - 1] + fSorted[fSorted.length / 2]) / 2
		: fSorted[Math.floor(fSorted.length / 2)]

	return {
		mean: Math.round(fm * 100) / 100,
		median: Math.round(fMedian * 100) / 100,
		std_dev: Math.round(Math.sqrt(fv) * 100) / 100,
		min: fSorted[0] ?? 0,
		max: fSorted[fSorted.length - 1] ?? 0,
		pass_rate: Math.round((fs.filter((s) => s >= threshold).length / fs.length) * 10000) / 100,
		total_graded: fc.length,
		total_failed: finalSubs.filter((s) => s.status === "failed").length,
		anomalies_regraded: regradeCount,
	}
}

export async function runGradingPipeline(
	supabase: SupabaseClient,
	session: GradingSession,
	submissions: Submission[],
): Promise<void> {
	const rubric = session.rubricStructured
	if (!rubric) throw new Error("Session has no parsed rubric")

	await supabase.from("grading_sessions").update({ status: "calibrating" }).eq("id", session.id)
	const anchors = await establishAnchors(supabase, session, submissions, rubric)

	await supabase.from("grading_sessions").update({ status: "grading" }).eq("id", session.id)
	await bulkGrade(supabase, session, submissions, rubric, anchors)

	await supabase.from("grading_sessions").update({ status: "normalizing" }).eq("id", session.id)
	const stats = await detectAndRegrade(supabase, session, rubric, anchors)

	await supabase.from("grading_sessions").update({ status: "completed", stats }).eq("id", session.id)
}
