import { NextResponse } from "next/server"
import { createServerSupabaseClient } from "@/features/v1/lib/supabase/server"
import { runGradingPipeline } from "@/features/v1/backend/grading/services/grading-pipeline"
import type { GradingSession, Submission, StructuredRubric, GradingSessionStatus, AnchorLevel, CriterionScore } from "@/features/v1/backend/grading/models/grading-types"

function mapSession(row: Record<string, unknown>): GradingSession {
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

function mapSubmission(row: Record<string, unknown>): Submission {
	return {
		id: row.id as string,
		sessionId: row.session_id as string,
		studentIdentifier: row.student_identifier as string,
		filePath: row.file_path as string,
		contentExtracted: row.content_extracted as string | null,
		status: row.status as Submission["status"],
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

export async function POST(request: Request) {
	try {
		const { sessionId } = await request.json()
		if (!sessionId) {
			return NextResponse.json({ error: "sessionId is required" }, { status: 400 })
		}

		const supabase = await createServerSupabaseClient()
		const { data: { user } } = await supabase.auth.getUser()
		if (!user) {
			return NextResponse.json({ error: "Unauthorized" }, { status: 401 })
		}

		const { data: sessionRow, error: sessionError } = await supabase
			.from("grading_sessions")
			.select("*")
			.eq("id", sessionId)
			.eq("user_id", user.id)
			.single()

		if (sessionError || !sessionRow) {
			return NextResponse.json({ error: "Session not found" }, { status: 404 })
		}

		const session = mapSession(sessionRow)

		const { data: subRows, error: subError } = await supabase
			.from("submissions")
			.select("*")
			.eq("session_id", sessionId)

		if (subError) {
			return NextResponse.json({ error: "Failed to fetch submissions" }, { status: 500 })
		}

		const submissions = (subRows || []).map(mapSubmission)

		if (submissions.length === 0) {
			return NextResponse.json({ error: "No submissions found" }, { status: 400 })
		}

		await runGradingPipeline(supabase as never, session, submissions)

		return NextResponse.json({ success: true })
	} catch (error) {
		const message = error instanceof Error ? error.message : "Pipeline failed"

		try {
			const body = await request.clone().json()
			if (body.sessionId) {
				const supabase = await createServerSupabaseClient()
				await supabase.from("grading_sessions")
					.update({ status: "failed" })
					.eq("id", body.sessionId)
			}
		} catch {
			// Best-effort status update
		}

		return NextResponse.json({ error: message }, { status: 500 })
	}
}
