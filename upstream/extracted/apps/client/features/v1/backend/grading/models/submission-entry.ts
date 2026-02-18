"use server"

import { createServerSupabaseClient } from "@/features/v1/lib/supabase/server"
import type { Submission, SubmissionStatus, CriterionScore, AnchorLevel } from "./grading-types"

function mapRow(row: Record<string, unknown>): Submission {
	return {
		id: row.id as string,
		sessionId: row.session_id as string,
		studentIdentifier: row.student_identifier as string,
		filePath: row.file_path as string,
		contentExtracted: row.content_extracted as string | null,
		status: row.status as SubmissionStatus,
		isAnchor: row.is_anchor as boolean,
		anchorLevel: row.anchor_level as AnchorLevel | null,
		scores: row.scores as CriterionScore[] | null,
		totalScore: row.total_score as number | null,
		feedback: row.feedback as string | null,
		improvementComments: row.improvement_comments as string | null,
		reasoning: row.reasoning as string | null,
		gradedAt: row.graded_at ? new Date(row.graded_at as string) : null,
		createdAt: new Date(row.created_at as string),
	}
}

export async function handleGetSubmissions(sessionId: string): Promise<Submission[]> {
	const supabase = await createServerSupabaseClient()

	const { data, error } = await supabase
		.from("submissions")
		.select("*")
		.eq("session_id", sessionId)
		.order("student_identifier", { ascending: true })

	if (error) throw new Error(`Failed to fetch submissions: ${error.message}`)
	return (data || []).map(mapRow)
}

export async function handleGetSubmission(submissionId: string): Promise<Submission> {
	const supabase = await createServerSupabaseClient()

	const { data, error } = await supabase
		.from("submissions")
		.select("*")
		.eq("id", submissionId)
		.single()

	if (error) throw new Error(`Submission not found: ${error.message}`)
	return mapRow(data)
}

export async function handleBulkCreateSubmissions(
	sessionId: string,
	submissions: Array<{
		studentIdentifier: string
		filePath: string
		contentExtracted: string
	}>,
): Promise<Submission[]> {
	const supabase = await createServerSupabaseClient()

	const rows = submissions.map((s) => ({
		session_id: sessionId,
		student_identifier: s.studentIdentifier,
		file_path: s.filePath,
		content_extracted: s.contentExtracted,
	}))

	const { data, error } = await supabase
		.from("submissions")
		.insert(rows)
		.select()

	if (error) throw new Error(`Failed to create submissions: ${error.message}`)
	return (data || []).map(mapRow)
}

export async function handleUpdateSubmission(
	submissionId: string,
	updates: Partial<{
		status: SubmissionStatus
		contentExtracted: string
	}>,
): Promise<Submission> {
	const supabase = await createServerSupabaseClient()

	const dbUpdates: Record<string, unknown> = {}
	if (updates.status !== undefined) dbUpdates.status = updates.status
	if (updates.contentExtracted !== undefined) dbUpdates.content_extracted = updates.contentExtracted

	const { data, error } = await supabase
		.from("submissions")
		.update(dbUpdates)
		.eq("id", submissionId)
		.select()
		.single()

	if (error) throw new Error(`Failed to update submission: ${error.message}`)
	return mapRow(data)
}
