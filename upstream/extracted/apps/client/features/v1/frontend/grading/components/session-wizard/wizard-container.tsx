"use client"

import { useEffect } from "react"
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/features/v1/frontend/shared/ui/tabs"
import { useWizardState, wizardStore } from "../../stores/wizard-store"
import { StepAssignment } from "./step-assignment"
import { StepRubric } from "./step-rubric"
import { StepSubmissions } from "./step-submissions"
import { StepReview } from "./step-review"

const STEPS = [
	{ value: "assignment", label: "Assignment" },
	{ value: "rubric", label: "Rubric" },
	{ value: "submissions", label: "Submissions" },
	{ value: "review", label: "Review" },
]

export const WizardContainer = () => {
	const { currentStep } = useWizardState()

	useEffect(() => {
		wizardStore.reset()
	}, [])

	return (
		<div className="p-6 max-w-3xl mx-auto">
			<h1 className="text-2xl font-bold mb-6">New Grading Session</h1>

			<Tabs
				value={STEPS[currentStep].value}
				onValueChange={(value) => {
					const idx = STEPS.findIndex((s) => s.value === value)
					if (idx >= 0) wizardStore.goToStep(idx)
				}}
			>
				<TabsList className="w-full grid grid-cols-4 mb-6">
					{STEPS.map((step, idx) => (
						<TabsTrigger
							key={step.value}
							value={step.value}
							disabled={idx > currentStep}
							className="text-xs sm:text-sm"
						>
							{idx + 1}. {step.label}
						</TabsTrigger>
					))}
				</TabsList>

				<TabsContent value="assignment">
					<StepAssignment />
				</TabsContent>
				<TabsContent value="rubric">
					<StepRubric />
				</TabsContent>
				<TabsContent value="submissions">
					<StepSubmissions />
				</TabsContent>
				<TabsContent value="review">
					<StepReview />
				</TabsContent>
			</Tabs>
		</div>
	)
}
