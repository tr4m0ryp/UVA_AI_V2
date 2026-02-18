"use server"

import { createServerSupabaseClient } from "@/features/v1/lib/supabase/server"
import type { GradingSession, GradingSessionStatus, StructuredRubric } from "./grading-types"

function mapRow(row: Record<string, unknown>): GradingSession {
	return {
		id: row.id as string,
		userId: row.user_id as string,
		name: row.name as string,
		assignmentText: row.assignment_text as string,
		assignmentFilePath: row.assignment_file_path as string | null,
		rubricRaw: row.rubric_raw as string,
		rubricStructured: row.rubric_structured as StructuredRubric | null,
		modelAnswer: row.model_answer as string,
		modelAnswerFilePath: row.model_answer_file_path as string | null,
		selectedModel: row.selected_model as string | null,
		modelReasoning: row.model_reasoning as string | null,
		status: row.status as GradingSessionStatus,
		anchorData: row.anchor_data as GradingSession["anchorData"],
		stats: row.stats as GradingSession["stats"],
		totalSubmissions: row.total_submissions as number,
		gradedCount: row.graded_count as number,
		createdAt: new Date(row.created_at as string),
		updatedAt: new Date(row.updated_at as string),
	}
}

export async function handleCreateGradingSession(input: {
	name: string
	assignmentText?: string
	rubricRaw?: string
	modelAnswer?: string
}): Promise<GradingSession> {
	const supabase = await createServerSupabaseClient()
	const { data: { user } } = await supabase.auth.getUser()
	if (!user) throw new Error("Unauthorized")

	const { data, error } = await supabase
		.from("grading_sessions")
		.insert({
			user_id: user.id,
			name: input.name,
			assignment_text: input.assignmentText || "",
			rubric_raw: input.rubricRaw || "",
			model_answer: input.modelAnswer || "",
		})
		.select()
		.single()

	if (error) throw new Error(`Failed to create session: ${error.message}`)
	return mapRow(data)
}

export async function handleGetAllGradingSessions(): Promise<GradingSession[]> {
	const supabase = await createServerSupabaseClient()
	const { data: { user } } = await supabase.auth.getUser()
	if (!user) throw new Error("Unauthorized")

	const { data, error } = await supabase
		.from("grading_sessions")
		.select("*")
		.eq("user_id", user.id)
		.order("created_at", { ascending: false })

	if (error) throw new Error(`Failed to fetch sessions: ${error.message}`)
	return (data || []).map(mapRow)
}

export async function handleGetGradingSession(sessionId: string): Promise<GradingSession> {
	const supabase = await createServerSupabaseClient()
	const { data: { user } } = await supabase.auth.getUser()
	if (!user) throw new Error("Unauthorized")

	const { data, error } = await supabase
		.from("grading_sessions")
		.select("*")
		.eq("id", sessionId)
		.eq("user_id", user.id)
		.single()

	if (error) throw new Error(`Session not found: ${error.message}`)
	return mapRow(data)
}

export async function handleUpdateGradingSession(
	sessionId: string,
	updates: Partial<{
		name: string
		assignmentText: string
		assignmentFilePath: string
		rubricRaw: string
		rubricStructured: StructuredRubric
		modelAnswer: string
		modelAnswerFilePath: string
		selectedModel: string
		modelReasoning: string
		status: GradingSessionStatus
		totalSubmissions: number
	}>,
): Promise<GradingSession> {
	const supabase = await createServerSupabaseClient()
	const { data: { user } } = await supabase.auth.getUser()
	if (!user) throw new Error("Unauthorized")

	const dbUpdates: Record<string, unknown> = {}
	if (updates.name !== undefined) dbUpdates.name = updates.name
	if (updates.assignmentText !== undefined) dbUpdates.assignment_text = updates.assignmentText
	if (updates.assignmentFilePath !== undefined) dbUpdates.assignment_file_path = updates.assignmentFilePath
	if (updates.rubricRaw !== undefined) dbUpdates.rubric_raw = updates.rubricRaw
	if (updates.rubricStructured !== undefined) dbUpdates.rubric_structured = updates.rubricStructured
	if (updates.modelAnswer !== undefined) dbUpdates.model_answer = updates.modelAnswer
	if (updates.modelAnswerFilePath !== undefined) dbUpdates.model_answer_file_path = updates.modelAnswerFilePath
	if (updates.selectedModel !== undefined) dbUpdates.selected_model = updates.selectedModel
	if (updates.modelReasoning !== undefined) dbUpdates.model_reasoning = updates.modelReasoning
	if (updates.status !== undefined) dbUpdates.status = updates.status
	if (updates.totalSubmissions !== undefined) dbUpdates.total_submissions = updates.totalSubmissions

	const { data, error } = await supabase
		.from("grading_sessions")
		.update(dbUpdates)
		.eq("id", sessionId)
		.eq("user_id", user.id)
		.select()
		.single()

	if (error) throw new Error(`Failed to update session: ${error.message}`)
	return mapRow(data)
}

export async function handleDeleteGradingSession(sessionId: string): Promise<boolean> {
	const supabase = await createServerSupabaseClient()
	const { data: { user } } = await supabase.auth.getUser()
	if (!user) throw new Error("Unauthorized")

	const { error } = await supabase
		.from("grading_sessions")
		.delete()
		.eq("id", sessionId)
		.eq("user_id", user.id)

	if (error) throw new Error(`Failed to delete session: ${error.message}`)
	return true
}
