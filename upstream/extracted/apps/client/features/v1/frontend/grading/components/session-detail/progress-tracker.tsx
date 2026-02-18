"use client"

import type { GradingSession } from "@/features/v1/backend/grading/models/grading-types"

interface ProgressTrackerProps {
	session: GradingSession
}

const PHASE_ORDER = ["draft", "parsing_rubric", "calibrating", "grading", "normalizing", "completed"]
const PHASE_LABELS: Record<string, string> = {
	draft: "Draft",
	parsing_rubric: "Parsing Rubric",
	calibrating: "Anchor Calibration",
	grading: "Bulk Grading",
	normalizing: "Anomaly Detection",
	completed: "Complete",
	failed: "Failed",
}

export const ProgressTracker = ({ session }: ProgressTrackerProps) => {
	const progress =
		session.totalSubmissions > 0
			? Math.round((session.gradedCount / session.totalSubmissions) * 100)
			: 0

	const currentPhaseIdx = PHASE_ORDER.indexOf(session.status)
	const isFailed = session.status === "failed"
	const isActive = !isFailed && session.status !== "completed" && session.status !== "draft"

	const estimatedRemaining =
		isActive && session.gradedCount > 0 && session.totalSubmissions > session.gradedCount
			? Math.ceil((session.totalSubmissions - session.gradedCount) * 0.5)
			: null

	return (
		<div className="space-y-4">
			<div className="flex items-center gap-2 overflow-x-auto">
				{PHASE_ORDER.map((phase, idx) => {
					const isCompleted = currentPhaseIdx > idx
					const isCurrent = session.status === phase
					const isFailedPhase = isFailed && idx === currentPhaseIdx

					return (
						<div key={phase} className="flex items-center gap-1">
							{idx > 0 && (
								<div
									className={`h-px w-6 ${
										isCompleted ? "bg-primary" : "bg-muted"
									}`}
								/>
							)}
							<div
								className={`px-2 py-1 rounded text-xs whitespace-nowrap ${
									isFailedPhase
										? "bg-red-100 text-red-800 dark:bg-red-900 dark:text-red-200"
										: isCompleted
											? "bg-primary/10 text-primary"
											: isCurrent
												? "bg-primary text-primary-foreground"
												: "bg-muted text-muted-foreground"
								}`}
							>
								{PHASE_LABELS[phase]}
							</div>
						</div>
					)
				})}
			</div>

			{isActive && (
				<p className="text-sm font-medium text-primary">
					Current phase: {PHASE_LABELS[session.status] || session.status}
				</p>
			)}

			{session.modelReasoning && (
				<p className="text-xs text-muted-foreground p-2 bg-muted rounded-md">
					Model selection reasoning: {session.modelReasoning}
				</p>
			)}

			{isActive && session.totalSubmissions > 0 && (
				<div className="space-y-2">
					<div className="h-3 bg-muted rounded-full overflow-hidden">
						<div
							className="h-full bg-primary rounded-full transition-all duration-500"
							style={{ width: `${progress}%` }}
						/>
					</div>
					<div className="flex items-center justify-between text-sm">
						<span className="text-muted-foreground">
							{session.gradedCount} / {session.totalSubmissions} graded
						</span>
						<span className="font-medium">{progress}%</span>
					</div>
					{estimatedRemaining && (
						<p className="text-xs text-muted-foreground">
							Estimated time remaining: ~{estimatedRemaining} min
						</p>
					)}
				</div>
			)}

			{session.status === "completed" && session.stats && session.stats.anomalies_regraded > 0 && (
				<p className="text-xs text-muted-foreground">
					{session.stats.anomalies_regraded} anomalous submission{session.stats.anomalies_regraded !== 1 ? "s" : ""} were detected and re-graded for consistency.
				</p>
			)}

			{isFailed && (
				<p className="text-sm text-red-600 dark:text-red-400">
					Grading pipeline failed. Check logs for details.
				</p>
			)}
		</div>
	)
}
