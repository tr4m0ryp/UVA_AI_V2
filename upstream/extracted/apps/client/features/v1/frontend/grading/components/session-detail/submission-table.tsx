"use client"

import { useState, useMemo } from "react"
import { ScrollArea } from "@/features/v1/frontend/shared/ui/scroll-area"
import type { Submission, StructuredRubric } from "@/features/v1/backend/grading/models/grading-types"
import { SubmissionDetail } from "./submission-detail"

interface SubmissionTableProps {
	submissions: Submission[]
	rubric: StructuredRubric | null
}

type SortField = "student" | "score" | "status"
type SortDir = "asc" | "desc"

export const SubmissionTable = ({ submissions, rubric }: SubmissionTableProps) => {
	const [sortField, setSortField] = useState<SortField>("student")
	const [sortDir, setSortDir] = useState<SortDir>("asc")
	const [selectedId, setSelectedId] = useState<string | null>(null)

	const toggleSort = (field: SortField) => {
		if (sortField === field) {
			setSortDir(sortDir === "asc" ? "desc" : "asc")
		} else {
			setSortField(field)
			setSortDir("asc")
		}
	}

	const sorted = useMemo(() => {
		const copy = [...submissions]
		copy.sort((a, b) => {
			let cmp = 0
			switch (sortField) {
				case "student":
					cmp = a.studentIdentifier.localeCompare(b.studentIdentifier)
					break
				case "score":
					cmp = (a.totalScore ?? -1) - (b.totalScore ?? -1)
					break
				case "status":
					cmp = a.status.localeCompare(b.status)
					break
			}
			return sortDir === "asc" ? cmp : -cmp
		})
		return copy
	}, [submissions, sortField, sortDir])

	const selectedSubmission = selectedId
		? submissions.find((s) => s.id === selectedId) || null
		: null

	const sortIndicator = (field: SortField) => {
		if (sortField !== field) return ""
		return sortDir === "asc" ? " ^" : " v"
	}

	return (
		<div className="space-y-2">
			<h3 className="text-sm font-medium">Submissions ({submissions.length})</h3>

			<ScrollArea className="border rounded-lg">
				<table className="w-full text-sm">
					<thead>
						<tr className="border-b bg-muted/50">
							<th
								className="text-left p-2 cursor-pointer hover:bg-muted"
								onClick={() => toggleSort("student")}
							>
								Student{sortIndicator("student")}
							</th>
							<th
								className="text-right p-2 cursor-pointer hover:bg-muted w-24"
								onClick={() => toggleSort("score")}
							>
								Score{sortIndicator("score")}
							</th>
							{rubric?.criteria.map((c) => (
								<th
									key={c.name}
									className="text-right p-2 w-20 hidden lg:table-cell"
									title={c.name}
								>
									<span className="truncate block max-w-[80px]">{c.name}</span>
								</th>
							))}
							<th
								className="text-center p-2 cursor-pointer hover:bg-muted w-24"
								onClick={() => toggleSort("status")}
							>
								Status{sortIndicator("status")}
							</th>
							<th className="text-center p-2 w-16">Anchor</th>
						</tr>
					</thead>
					<tbody>
						{sorted.map((sub) => {
							const isPass =
								rubric && sub.totalScore !== null
									? sub.totalScore >= rubric.pass_threshold
									: null

							return (
								<tr
									key={sub.id}
									className="border-b hover:bg-muted/30 cursor-pointer"
									onClick={() => setSelectedId(sub.id)}
								>
									<td className="p-2 font-medium">{sub.studentIdentifier}</td>
									<td className="p-2 text-right">
										{sub.totalScore !== null ? (
											<span
												className={
													isPass === true
														? "text-green-600 dark:text-green-400"
														: isPass === false
															? "text-red-600 dark:text-red-400"
															: ""
												}
											>
												{sub.totalScore}
											</span>
										) : (
											<span className="text-muted-foreground">-</span>
										)}
									</td>
									{rubric?.criteria.map((c) => {
										const score = sub.scores?.find(
											(s) => s.criterion_name === c.name,
										)
										return (
											<td
												key={c.name}
												className="p-2 text-right text-muted-foreground hidden lg:table-cell"
											>
												{score ? score.score : "-"}
											</td>
										)
									})}
									<td className="p-2 text-center">
										<span
											className={`text-xs px-1.5 py-0.5 rounded ${
												sub.status === "completed"
													? "bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-200"
													: sub.status === "failed"
														? "bg-red-100 text-red-800 dark:bg-red-900 dark:text-red-200"
														: sub.status === "grading" || sub.status === "re_grading"
															? "bg-yellow-100 text-yellow-800 dark:bg-yellow-900 dark:text-yellow-200"
															: "bg-muted text-muted-foreground"
											}`}
										>
											{sub.status}
										</span>
									</td>
									<td className="p-2 text-center">
										{sub.isAnchor ? (
											<span className="text-xs px-1.5 py-0.5 rounded bg-blue-100 text-blue-800 dark:bg-blue-900 dark:text-blue-200">
												{sub.anchorLevel}
											</span>
										) : (
											""
										)}
									</td>
								</tr>
							)
						})}
					</tbody>
				</table>
			</ScrollArea>

			<SubmissionDetail
				submission={selectedSubmission}
				rubric={rubric}
				open={!!selectedId}
				onOpenChange={(open) => {
					if (!open) setSelectedId(null)
				}}
			/>
		</div>
	)
}
