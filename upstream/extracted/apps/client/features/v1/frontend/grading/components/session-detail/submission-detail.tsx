"use client"

import {
	Dialog,
	DialogContent,
	DialogHeader,
	DialogTitle,
	DialogDescription,
} from "@/features/v1/frontend/shared/ui/dialog"
import { ScrollArea } from "@/features/v1/frontend/shared/ui/scroll-area"
import type { Submission, StructuredRubric } from "@/features/v1/backend/grading/models/grading-types"
import { RubricViewer } from "../rubric/rubric-viewer"

interface SubmissionDetailProps {
	submission: Submission | null
	rubric: StructuredRubric | null
	open: boolean
	onOpenChange: (open: boolean) => void
}

export const SubmissionDetail = ({ submission, rubric, open, onOpenChange }: SubmissionDetailProps) => {
	if (!submission) return null

	return (
		<Dialog open={open} onOpenChange={onOpenChange}>
			<DialogContent className="max-w-3xl max-h-[90vh] flex flex-col">
				<DialogHeader>
					<DialogTitle className="flex items-center gap-2">
						{submission.studentIdentifier}
						{submission.isAnchor && (
							<span className="text-xs px-2 py-0.5 rounded-full bg-blue-100 text-blue-800 dark:bg-blue-900 dark:text-blue-200">
								{submission.anchorLevel} anchor
							</span>
						)}
					</DialogTitle>
					<DialogDescription>
						<span className="flex items-center gap-4">
							<span>
								Score: {submission.totalScore !== null ? submission.totalScore : "N/A"}
								{rubric && ` / ${rubric.total_max}`}
							</span>
							<span>Status: {submission.status}</span>
							{submission.gradedAt && (
								<span>Graded: {new Date(submission.gradedAt).toLocaleString()}</span>
							)}
						</span>
					</DialogDescription>
				</DialogHeader>
				<ScrollArea className="flex-1 pr-4">
					<div className="space-y-4">
						{rubric && submission.scores && (
							<div>
								<h3 className="text-sm font-medium mb-2">Score Breakdown</h3>
								<RubricViewer rubric={rubric} scores={submission.scores} />
							</div>
						)}

						{submission.feedback && (
							<div>
								<h3 className="text-sm font-medium mb-1">Feedback</h3>
								<p className="text-sm text-muted-foreground whitespace-pre-wrap">
									{submission.feedback}
								</p>
							</div>
						)}

						{submission.improvementComments && (
							<div>
								<h3 className="text-sm font-medium mb-1">Improvement Suggestions</h3>
								<p className="text-sm text-muted-foreground whitespace-pre-wrap">
									{submission.improvementComments}
								</p>
							</div>
						)}

						{submission.reasoning && (
							<div>
								<h3 className="text-sm font-medium mb-1">Grading Reasoning</h3>
								<p className="text-sm text-muted-foreground whitespace-pre-wrap bg-muted rounded-md p-3">
									{submission.reasoning}
								</p>
							</div>
						)}
					</div>
				</ScrollArea>
			</DialogContent>
		</Dialog>
	)
}
