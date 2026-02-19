"use client"

import { useEffect } from "react"
import { useParams } from "next/navigation"
import { useGradingState, gradingStore } from "@/features/v1/frontend/grading/stores/grading-store"
import { SessionHeader } from "@/features/v1/frontend/grading/components/session-detail/session-header"
import { ProgressTracker } from "@/features/v1/frontend/grading/components/session-detail/progress-tracker"
import { SubmissionTable } from "@/features/v1/frontend/grading/components/session-detail/submission-table"
import { ScoreDistribution } from "@/features/v1/frontend/grading/components/session-detail/score-distribution"
import { ExportPanel } from "@/features/v1/frontend/grading/components/session-detail/export-panel"
import { ACTIVE_STATUSES } from "@/features/v1/frontend/grading/constants"
import type { AnchorData } from "@/features/v1/backend/grading/models/grading-types"

function AnchorSection({ anchorData }: { anchorData: AnchorData }) {
	const levels: Array<{ key: keyof AnchorData; label: string; color: string }> = [
		{ key: "high", label: "HIGH", color: "border-green-300 dark:border-green-700" },
		{ key: "mid", label: "MID", color: "border-yellow-300 dark:border-yellow-700" },
		{ key: "low", label: "LOW", color: "border-red-300 dark:border-red-700" },
	]

	return (
		<div className="space-y-3">
			<h3 className="text-sm font-medium">Anchor Submissions</h3>
			<p className="text-xs text-muted-foreground">
				These submissions were used as calibration anchors during grading.
			</p>
			<div className="grid gap-3 sm:grid-cols-3">
				{levels.map(({ key, label, color }) => {
					const anchor = anchorData[key]
					if (!anchor) return null
					return (
						<div key={key} className={`border-l-4 ${color} p-3 bg-muted/30 rounded-r-md`}>
							<div className="flex items-center justify-between mb-1">
								<span className="text-xs font-medium px-1.5 py-0.5 rounded bg-blue-100 text-blue-800 dark:bg-blue-900 dark:text-blue-200">
									{label}
								</span>
								<span className="text-sm font-bold">{anchor.total_score}</span>
							</div>
							<p className="text-xs text-muted-foreground">
								{anchor.student_identifier}
							</p>
							{anchor.content_excerpt && (
								<p className="text-xs text-muted-foreground mt-1 line-clamp-3">
									{anchor.content_excerpt}
								</p>
							)}
						</div>
					)
				})}
			</div>
		</div>
	)
}

export default function GradingSessionDetailPage() {
	const params = useParams()
	const sessionId = params.sessionId as string
	const { currentSession, submissions, isLoading } = useGradingState()

	useEffect(() => {
		if (sessionId) {
			gradingStore.loadSession(sessionId)
		}

		return () => {
			gradingStore.stopPolling()
		}
	}, [sessionId])

	useEffect(() => {
		if (
			currentSession &&
			ACTIVE_STATUSES.includes(currentSession.status as typeof ACTIVE_STATUSES[number])
		) {
			gradingStore.startPolling(sessionId)
		}
	}, [currentSession?.status, sessionId])

	if (isLoading && !currentSession) {
		return (
			<div className="flex items-center justify-center h-48 text-muted-foreground">
				Loading session...
			</div>
		)
	}

	if (!currentSession) {
		return (
			<div className="flex items-center justify-center h-48 text-muted-foreground">
				Session not found
			</div>
		)
	}

	const rubric = currentSession.rubricStructured

	return (
		<div className="p-6 max-w-6xl mx-auto space-y-6">
			<SessionHeader session={currentSession} />
			<ProgressTracker session={currentSession} />

			{currentSession.status === "completed" && currentSession.anchorData && (
				<AnchorSection anchorData={currentSession.anchorData} />
			)}

			{currentSession.status === "completed" && rubric && (
				<ScoreDistribution
					submissions={submissions}
					totalMax={rubric.total_max}
					passThreshold={rubric.pass_threshold}
				/>
			)}

			<ExportPanel session={currentSession} />

			{submissions.length > 0 && (
				<SubmissionTable submissions={submissions} rubric={rubric} />
			)}
		</div>
	)
}
