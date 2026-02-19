import { NextResponse } from "next/server"
import { createServerSupabaseClient } from "@/features/v1/lib/supabase/server"
import { parseRubric } from "@/features/v1/backend/grading/services/rubric-parser"

export async function POST(request: Request) {
	try {
		const { rubricText } = await request.json()
		if (!rubricText || typeof rubricText !== "string") {
			return NextResponse.json(
				{ error: "rubricText is required and must be a string" },
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

		const structured = await parseRubric(rubricText)
		return NextResponse.json(structured)
	} catch (error) {
		const message = error instanceof Error ? error.message : "Failed to parse rubric"
		return NextResponse.json({ error: message }, { status: 500 })
	}
}
