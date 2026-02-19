"use client"

import Link from "next/link"
import type { GradingSession } from "@/features/v1/backend/grading/models/grading-types"
import { STATUS_LABELS, STATUS_COLORS } from "../../constants"

interface SessionHeaderProps {
	session: GradingSession
}

export const SessionHeader = ({ session }: SessionHeaderProps) => {
	return (
		<div className="space-y-1">
			<div className="flex items-center gap-3">
				<Link href="/grading" className="text-muted-foreground hover:text-foreground text-sm">
					Sessions
				</Link>
				<span className="text-muted-foreground">/</span>
				<h1 className="text-xl font-bold">{session.name}</h1>
			</div>
			<div className="flex items-center gap-3 text-sm text-muted-foreground">
				<span
					className={`px-2 py-0.5 rounded-full text-xs ${
						STATUS_COLORS[session.status] || STATUS_COLORS.draft
					}`}
				>
					{STATUS_LABELS[session.status] || session.status}
				</span>
				{session.selectedModel && <span>Model: {session.selectedModel}</span>}
				<span>{session.totalSubmissions} submissions</span>
			</div>
		</div>
	)
}
