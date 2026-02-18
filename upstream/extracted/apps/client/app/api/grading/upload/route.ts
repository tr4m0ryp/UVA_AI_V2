import { NextResponse } from "next/server"
import { createServerSupabaseClient } from "@/features/v1/lib/supabase/server"
import { extractTextFromBuffer } from "@/features/v1/backend/grading/services/file-extractor"

interface UploadResult {
	studentIdentifier: string
	filePath: string
	contentExtracted: string
}

export async function POST(request: Request) {
	try {
		const supabase = await createServerSupabaseClient()
		const {
			data: { user },
		} = await supabase.auth.getUser()
		if (!user) {
			return NextResponse.json({ error: "Unauthorized" }, { status: 401 })
		}

		const formData = await request.formData()
		const sessionId = formData.get("sessionId") as string
		if (!sessionId) {
			return NextResponse.json({ error: "sessionId is required" }, { status: 400 })
		}

		const files = formData.getAll("files") as File[]
		if (files.length === 0) {
			return NextResponse.json({ error: "At least one file is required" }, { status: 400 })
		}

		const results: UploadResult[] = []

		for (const file of files) {
			const arrayBuffer = await file.arrayBuffer()
			const buffer = Buffer.from(arrayBuffer)
			const storagePath = `${user.id}/${sessionId}/${file.name}`

			const { error: uploadError } = await supabase.storage
				.from("grading-files")
				.upload(storagePath, buffer, {
					contentType: file.type || "application/octet-stream",
					upsert: true,
				})

			if (uploadError) {
				throw new Error(`Failed to upload ${file.name}: ${uploadError.message}`)
			}

			const extracted = await extractTextFromBuffer(buffer, file.name)

			results.push({
				studentIdentifier: extracted.studentIdentifier,
				filePath: storagePath,
				contentExtracted: extracted.content,
			})
		}

		return NextResponse.json(results)
	} catch (error) {
		const message = error instanceof Error ? error.message : "Upload failed"
		return NextResponse.json({ error: message }, { status: 500 })
	}
}
