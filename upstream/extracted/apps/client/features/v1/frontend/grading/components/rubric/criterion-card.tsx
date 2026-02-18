"use client"

import { Card, CardContent, CardHeader, CardTitle } from "@/features/v1/frontend/shared/ui/card"
import type { RubricCriterion } from "@/features/v1/backend/grading/models/grading-types"

interface CriterionCardProps {
	criterion: RubricCriterion
	score?: number
}

export const CriterionCard = ({ criterion, score }: CriterionCardProps) => {
	return (
		<Card className="mb-3">
			<CardHeader className="pb-2">
				<div className="flex items-center justify-between">
					<CardTitle className="text-sm font-medium">{criterion.name}</CardTitle>
					<span className="text-sm text-muted-foreground">
						{score !== undefined ? `${score}/` : ""}{criterion.max_points} pts
					</span>
				</div>
			</CardHeader>
			<CardContent>
				<div className="space-y-1.5">
					{criterion.levels.map((level) => {
						const isActive =
							score !== undefined &&
							score >= level.range[0] &&
							score <= level.range[1]

						return (
							<div
								key={level.label}
								className={`rounded-md border p-2 text-xs ${
									isActive
										? "border-primary bg-primary/5"
										: "border-transparent"
								}`}
							>
								<div className="flex items-center justify-between mb-0.5">
									<span className="font-medium">{level.label}</span>
									<span className="text-muted-foreground">
										{level.range[0]}-{level.range[1]}
									</span>
								</div>
								<p className="text-muted-foreground">{level.description}</p>
							</div>
						)
					})}
				</div>
			</CardContent>
		</Card>
	)
}
