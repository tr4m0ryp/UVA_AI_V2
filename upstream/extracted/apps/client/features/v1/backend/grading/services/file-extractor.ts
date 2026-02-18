import pdf from "pdf-parse"

interface ExtractedFile {
	studentIdentifier: string
	content: string
}

const FILENAME_PATTERN = /^([^_]+)_(.+)\.(pdf|txt)$/

function parseStudentId(filename: string): string {
	const match = filename.match(FILENAME_PATTERN)
	if (match) {
		return match[1]
	}
	const dotIdx = filename.lastIndexOf(".")
	return dotIdx > 0 ? filename.slice(0, dotIdx) : filename
}

export async function extractTextFromBuffer(
	buffer: Buffer,
	filename: string,
): Promise<ExtractedFile> {
	const studentIdentifier = parseStudentId(filename)
	const ext = filename.split(".").pop()?.toLowerCase()

	let content: string

	if (ext === "pdf") {
		const parsed = await pdf(buffer)
		content = parsed.text
	} else {
		content = buffer.toString("utf-8")
	}

	content = content.trim()
	if (!content) {
		throw new Error(`Empty content extracted from ${filename}`)
	}

	return { studentIdentifier, content }
}

export async function extractTextFromBlob(
	blob: Blob,
	filename: string,
): Promise<ExtractedFile> {
	const arrayBuffer = await blob.arrayBuffer()
	const buffer = Buffer.from(arrayBuffer)
	return extractTextFromBuffer(buffer, filename)
}
