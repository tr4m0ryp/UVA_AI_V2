"use client"

import { useState } from "react"
import Link from "next/link"
import { Card, CardContent, CardHeader, CardTitle } from "@/features/v1/frontend/shared/ui/card"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import {
	Dialog,
	DialogContent,
	DialogHeader,
	DialogTitle,
	DialogDescription,
	DialogFooter,
} from "@/features/v1/frontend/shared/ui/dialog"
import type { GradingSession } from "@/features/v1/backend/grading/models/grading-types"
import { gradingStore } from "../../stores/grading-store"
import { STATUS_LABELS, STATUS_COLORS, ACTIVE_STATUSES } from "../../constants"

interface SessionCardProps {
	session: GradingSession
}

export const SessionCard = ({ session }: SessionCardProps) => {
	const [confirmOpen, setConfirmOpen] = useState(false)

	const progress =
		session.totalSubmissions > 0
			? Math.round((session.gradedCount / session.totalSubmissions) * 100)
			: 0

	const isActive = ACTIVE_STATUSES.includes(session.status as typeof ACTIVE_STATUSES[number])

	const handleDelete = async () => {
		await gradingStore.deleteSession(session.id)
		setConfirmOpen(false)
	}

	return (
		<>
			<Link href={`/grading/${session.id}`}>
				<Card className="hover:shadow-md transition-shadow cursor-pointer">
					<CardHeader className="pb-2">
						<div className="flex items-center justify-between">
							<CardTitle className="text-base truncate">{session.name}</CardTitle>
							<span
								className={`text-xs px-2 py-0.5 rounded-full whitespace-nowrap ${
									STATUS_COLORS[session.status] || STATUS_COLORS.draft
								}`}
							>
								{STATUS_LABELS[session.status] || session.status}
							</span>
						</div>
					</CardHeader>
					<CardContent>
						<div className="space-y-2">
							<div className="flex justify-between text-sm text-muted-foreground">
								<span>{session.totalSubmissions} submissions</span>
								{session.selectedModel && (
									<span>{session.selectedModel}</span>
								)}
							</div>

							{isActive && session.totalSubmissions > 0 && (
								<div className="space-y-1">
									<div className="h-2 bg-muted rounded-full overflow-hidden">
										<div
											className="h-full bg-primary rounded-full transition-all"
											style={{ width: `${progress}%` }}
										/>
									</div>
									<p className="text-xs text-muted-foreground text-right">
										{session.gradedCount}/{session.totalSubmissions} ({progress}%)
									</p>
								</div>
							)}

							{session.stats && session.status === "completed" && (
								<div className="text-xs text-muted-foreground flex gap-3">
									<span>Mean: {session.stats.mean}</span>
									<span>Pass: {session.stats.pass_rate}%</span>
								</div>
							)}

							<div className="flex items-center justify-between">
								<span className="text-xs text-muted-foreground">
									{session.createdAt.toLocaleDateString()}
								</span>
								{session.status === "draft" && (
									<Button
										variant="ghost"
										size="sm"
										className="text-xs text-destructive h-6"
										onClick={(e) => {
											e.preventDefault()
											e.stopPropagation()
											setConfirmOpen(true)
										}}
									>
										Delete
									</Button>
								)}
							</div>
						</div>
					</CardContent>
				</Card>
			</Link>

			<Dialog open={confirmOpen} onOpenChange={setConfirmOpen}>
				<DialogContent>
					<DialogHeader>
						<DialogTitle>Delete Session</DialogTitle>
						<DialogDescription>
							Are you sure you want to delete &quot;{session.name}&quot;? This action cannot be undone.
						</DialogDescription>
					</DialogHeader>
					<DialogFooter>
						<Button variant="outline" onClick={() => setConfirmOpen(false)}>
							Cancel
						</Button>
						<Button variant="destructive" onClick={handleDelete}>
							Delete
						</Button>
					</DialogFooter>
				</DialogContent>
			</Dialog>
		</>
	)
}
