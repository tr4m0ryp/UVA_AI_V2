import { NextResponse } from "next/server"
import { createServerSupabaseClient } from "@/features/v1/lib/supabase/server"
import { selectModel } from "@/features/v1/backend/grading/services/model-selector"

export async function POST(request: Request) {
	try {
		const { assignmentText, rubricSummary } = await request.json()
		if (!assignmentText || !rubricSummary) {
			return NextResponse.json(
				{ error: "assignmentText and rubricSummary are required" },
				{ status: 400 },
			)
		}

		const supabase = await createServerSupabaseClient()
		const {
			data: { user },
		} = await supabase.auth.getUser()
		if (!user) {
			return NextResponse.json({ error: "Unauthorized" }, { status: 401 })
		}

		const result = await selectModel(assignmentText, rubricSummary)
		return NextResponse.json(result)
	} catch (error) {
		const message = error instanceof Error ? error.message : "Failed to select model"
		return NextResponse.json({ error: message }, { status: 500 })
	}
}
