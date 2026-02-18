"use client"

import { ScrollArea } from "@/features/v1/frontend/shared/ui/scroll-area"
import type { StructuredRubric, CriterionScore } from "@/features/v1/backend/grading/models/grading-types"
import { CriterionCard } from "./criterion-card"

interface RubricViewerProps {
	rubric: StructuredRubric
	scores?: CriterionScore[]
}

export const RubricViewer = ({ rubric, scores }: RubricViewerProps) => {
	return (
		<div className="space-y-4">
			<div className="flex items-center justify-between text-sm">
				<span className="font-medium">
					{rubric.criteria.length} criteria
				</span>
				<span className="text-muted-foreground">
					Total: {rubric.total_max} pts | Pass: {rubric.pass_threshold} pts
				</span>
			</div>

			<ScrollArea className="h-[400px] pr-4">
				{rubric.criteria.map((criterion) => {
					const criterionScore = scores?.find(
						(s) => s.criterion_name === criterion.name,
					)

					return (
						<CriterionCard
							key={criterion.name}
							criterion={criterion}
							score={criterionScore?.score}
						/>
					)
				})}
			</ScrollArea>
		</div>
	)
}
