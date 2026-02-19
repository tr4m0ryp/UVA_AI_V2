import { jsPDF } from "jspdf"
import type { CriterionScore, Submission, GradingSession, StructuredRubric } from "../models/grading-types"

function escapeCSV(value: string): string {
	if (value.includes(",") || value.includes('"') || value.includes("\n")) {
		return `"${value.replace(/"/g, '""')}"`
	}
	return value
}

export function generateCSV(
	session: GradingSession,
	submissions: Submission[],
): string {
	const rubric = session.rubricStructured
	const criteriaNames = rubric?.criteria.map((c) => c.name) || []

	const headers = [
		"Student",
		"Total Score",
		...criteriaNames,
		"Feedback",
		"Improvement Comments",
	]

	const rows = submissions
		.filter((s) => s.status === "completed")
		.map((sub) => {
			const criteriaScores = criteriaNames.map((name) => {
				const score = sub.scores?.find((s: CriterionScore) => s.criterion_name === name)
				return score ? String(score.score) : ""
			})

			return [
				escapeCSV(sub.studentIdentifier),
				sub.totalScore !== null ? String(sub.totalScore) : "",
				...criteriaScores,
				escapeCSV(sub.feedback || ""),
				escapeCSV(sub.improvementComments || ""),
			]
		})

	const lines = [headers.map(escapeCSV).join(",")]
	for (const row of rows) {
		lines.push(row.join(","))
	}

	return lines.join("\n")
}

export function generatePDF(
	session: GradingSession,
	submissions: Submission[],
): Buffer {
	const doc = new jsPDF()
	const rubric = session.rubricStructured as StructuredRubric | null
	const completed = submissions.filter((s) => s.status === "completed")

	doc.setFontSize(16)
	doc.text(session.name, 14, 20)
	doc.setFontSize(10)
	doc.text(`Total submissions: ${completed.length}`, 14, 28)

	if (session.stats) {
		doc.text(`Mean: ${session.stats.mean} | Median: ${session.stats.median} | Pass rate: ${session.stats.pass_rate}%`, 14, 34)
	}

	let yPos = 44

	for (const sub of completed) {
		if (yPos > 250) {
			doc.addPage()
			yPos = 20
		}

		doc.setFontSize(12)
		doc.text(`${sub.studentIdentifier} - Score: ${sub.totalScore ?? "N/A"}`, 14, yPos)
		yPos += 8

		if (sub.scores && rubric) {
			doc.setFontSize(9)
			for (const score of sub.scores) {
				const criterion = rubric.criteria.find((c) => c.name === score.criterion_name)
				const maxPts = criterion?.max_points ?? score.max_points
				doc.text(`  ${score.criterion_name}: ${score.score}/${maxPts}`, 14, yPos)
				yPos += 5
			}
		}

		if (sub.feedback) {
			doc.setFontSize(9)
			const feedbackLines = doc.splitTextToSize(`Feedback: ${sub.feedback}`, 180)
			doc.text(feedbackLines, 14, yPos)
			yPos += feedbackLines.length * 4 + 2
		}

		if (sub.improvementComments) {
			const improvLines = doc.splitTextToSize(`Improvements: ${sub.improvementComments}`, 180)
			doc.text(improvLines, 14, yPos)
			yPos += improvLines.length * 4 + 2
		}

		yPos += 6
	}

	return Buffer.from(doc.output("arraybuffer"))
}
