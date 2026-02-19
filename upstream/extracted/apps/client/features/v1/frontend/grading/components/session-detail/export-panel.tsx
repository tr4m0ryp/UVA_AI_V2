"use client"

import { Button } from "@/features/v1/frontend/shared/ui/button"
import type { GradingSession } from "@/features/v1/backend/grading/models/grading-types"

interface ExportPanelProps {
	session: GradingSession
}

export const ExportPanel = ({ session }: ExportPanelProps) => {
	if (session.status !== "completed") return null

	return (
		<div className="space-y-2">
			<h3 className="text-sm font-medium">Export Results</h3>
			<div className="flex items-center gap-3">
				<a
					href={`/api/grading/export?session_id=${session.id}&format=csv`}
					download
				>
					<Button variant="outline" size="sm">
						Download CSV
					</Button>
				</a>
				<a
					href={`/api/grading/export?session_id=${session.id}&format=pdf`}
					download
				>
					<Button variant="outline" size="sm">
						Download PDF
					</Button>
				</a>
			</div>
			{session.stats && (
				<div className="grid grid-cols-3 sm:grid-cols-5 gap-3 mt-3">
					<div className="text-center p-2 bg-muted rounded-md">
						<p className="text-lg font-bold">{session.stats.mean}</p>
						<p className="text-xs text-muted-foreground">Mean</p>
					</div>
					<div className="text-center p-2 bg-muted rounded-md">
						<p className="text-lg font-bold">{session.stats.median}</p>
						<p className="text-xs text-muted-foreground">Median</p>
					</div>
					<div className="text-center p-2 bg-muted rounded-md">
						<p className="text-lg font-bold">{session.stats.pass_rate}%</p>
						<p className="text-xs text-muted-foreground">Pass Rate</p>
					</div>
					<div className="text-center p-2 bg-muted rounded-md">
						<p className="text-lg font-bold">{session.stats.std_dev}</p>
						<p className="text-xs text-muted-foreground">Std Dev</p>
					</div>
					<div className="text-center p-2 bg-muted rounded-md">
						<p className="text-lg font-bold">{session.stats.min}</p>
						<p className="text-xs text-muted-foreground">Min</p>
					</div>
					<div className="text-center p-2 bg-muted rounded-md">
						<p className="text-lg font-bold">{session.stats.max}</p>
						<p className="text-xs text-muted-foreground">Max</p>
					</div>
					<div className="text-center p-2 bg-muted rounded-md">
						<p className="text-lg font-bold">{session.stats.total_graded}</p>
						<p className="text-xs text-muted-foreground">Graded</p>
					</div>
					<div className="text-center p-2 bg-muted rounded-md">
						<p className="text-lg font-bold">{session.stats.total_failed}</p>
						<p className="text-xs text-muted-foreground">Failed</p>
					</div>
					<div className="text-center p-2 bg-muted rounded-md">
						<p className="text-lg font-bold">{session.stats.anomalies_regraded}</p>
						<p className="text-xs text-muted-foreground">Regraded</p>
					</div>
				</div>
			)}
		</div>
	)
}
