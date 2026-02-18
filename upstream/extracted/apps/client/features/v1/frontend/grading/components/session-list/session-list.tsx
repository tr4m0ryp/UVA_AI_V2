"use client"

import { useEffect } from "react"
import Link from "next/link"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import { useGradingState, initializeGradingData } from "../../stores/grading-store"
import { SessionCard } from "./session-card"

export const SessionList = () => {
	const { sessions, isLoading } = useGradingState()

	useEffect(() => {
		initializeGradingData()
	}, [])

	return (
		<div className="p-6 max-w-6xl mx-auto">
			<div className="flex items-center justify-between mb-6">
				<div>
					<h1 className="text-2xl font-bold">Grading Sessions</h1>
					<p className="text-sm text-muted-foreground mt-1">
						AI-powered consistent grading across submissions
					</p>
				</div>
				<Link href="/grading/new">
					<Button>New Session</Button>
				</Link>
			</div>

			{isLoading && sessions.length === 0 ? (
				<div className="flex items-center justify-center h-48 text-muted-foreground">
					Loading sessions...
				</div>
			) : sessions.length === 0 ? (
				<div className="flex flex-col items-center justify-center h-48 text-muted-foreground gap-3">
					<p>No grading sessions yet</p>
					<Link href="/grading/new">
						<Button variant="outline">Create your first session</Button>
					</Link>
				</div>
			) : (
				<div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
					{sessions.map((session) => (
						<SessionCard key={session.id} session={session} />
					))}
				</div>
			)}
		</div>
	)
}
