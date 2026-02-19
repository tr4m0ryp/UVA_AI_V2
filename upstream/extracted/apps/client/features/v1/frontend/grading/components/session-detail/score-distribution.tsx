"use client"

import type { Submission } from "@/features/v1/backend/grading/models/grading-types"

interface ScoreDistributionProps {
	submissions: Submission[]
	totalMax: number
	passThreshold: number
}

export const ScoreDistribution = ({
	submissions,
	totalMax,
	passThreshold,
}: ScoreDistributionProps) => {
	const completed = submissions.filter(
		(s) => s.status === "completed" && s.totalScore !== null,
	)

	if (completed.length === 0) return null

	const bucketCount = 10
	const bucketSize = totalMax / bucketCount
	const buckets: number[] = new Array(bucketCount).fill(0)

	for (const sub of completed) {
		const score = sub.totalScore as number
		const idx = Math.min(Math.floor(score / bucketSize), bucketCount - 1)
		buckets[idx]++
	}

	const maxCount = Math.max(...buckets, 1)
	const passIdx = Math.floor(passThreshold / bucketSize)

	return (
		<div className="space-y-2">
			<h3 className="text-sm font-medium">Score Distribution</h3>
			<div className="flex items-end gap-1 h-24">
				{buckets.map((count, idx) => {
					const height = (count / maxCount) * 100
					const rangeStart = Math.round(idx * bucketSize)
					const rangeEnd = Math.round((idx + 1) * bucketSize)
					const isPassing = idx >= passIdx

					return (
						<div
							key={idx}
							className="flex-1 flex flex-col items-center gap-1"
							title={`${rangeStart}-${rangeEnd}: ${count} submissions`}
						>
							{count > 0 && (
								<span className="text-[10px] text-muted-foreground">{count}</span>
							)}
							<div
								className={`w-full rounded-t transition-all ${
									isPassing
										? "bg-green-400 dark:bg-green-600"
										: "bg-red-300 dark:bg-red-700"
								}`}
								style={{ height: `${height}%`, minHeight: count > 0 ? "4px" : "0" }}
							/>
						</div>
					)
				})}
			</div>
			<div className="flex justify-between text-[10px] text-muted-foreground">
				<span>0</span>
				<span className="text-center">Pass: {passThreshold}</span>
				<span>{totalMax}</span>
			</div>
		</div>
	)
}
