import { NextResponse } from "next/server"
import { createServerSupabaseClient } from "@/features/v1/lib/supabase/server"
import { generateCSV, generatePDF } from "@/features/v1/backend/grading/services/export-service"
import type { GradingSession, StructuredRubric, GradingSessionStatus, Submission, AnchorLevel, CriterionScore } from "@/features/v1/backend/grading/models/grading-types"

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

export async function GET(request: Request) {
	const { searchParams } = new URL(request.url)
	const sessionId = searchParams.get("session_id")
	const format = searchParams.get("format") || "csv"

	if (!sessionId) {
		return NextResponse.json({ error: "session_id is required" }, { status: 400 })
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

	const { data: subRows } = await supabase
		.from("submissions")
		.select("*")
		.eq("session_id", sessionId)
		.order("student_identifier", { ascending: true })

	const submissions = (subRows || []).map(mapSubmission)
	const safeName = session.name.replace(/[^a-zA-Z0-9-_]/g, "_")

	if (format === "pdf") {
		const pdfBuffer = generatePDF(session, submissions)
		return new NextResponse(new Uint8Array(pdfBuffer), {
			headers: {
				"Content-Type": "application/pdf",
				"Content-Disposition": `attachment; filename="${safeName}_grades.pdf"`,
			},
		})
	}

	const csv = generateCSV(session, submissions)
	return new NextResponse(csv, {
		headers: {
			"Content-Type": "text/csv",
			"Content-Disposition": `attachment; filename="${safeName}_grades.csv"`,
		},
	})
}
