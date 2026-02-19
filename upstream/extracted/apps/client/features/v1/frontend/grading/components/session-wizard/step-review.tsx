"use client"

import { useEffect } from "react"
import { useRouter } from "next/navigation"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import { Card, CardContent, CardHeader, CardTitle } from "@/features/v1/frontend/shared/ui/card"
import { Label } from "@/features/v1/frontend/shared/ui/label"
import { useWizardState, wizardStore } from "../../stores/wizard-store"
import { MODEL_LABELS } from "../../constants"

const PHASE_MESSAGES: Record<string, string> = {
	uploading: "Uploading files...",
	creating: "Creating submissions...",
	starting: "Starting grading pipeline...",
	done: "Done!",
}

export const StepReview = () => {
	const {
		sessionName,
		assignmentText,
		modelAnswer,
		rubricParsed,
		submissionFiles,
		selectedModel,
		modelReasoning,
		isSelectingModel,
		isLaunching,
		launchPhase,
	} = useWizardState()
	const router = useRouter()

	useEffect(() => {
		if (!selectedModel && !isSelectingModel) {
			wizardStore.selectModel()
		}
	}, [selectedModel, isSelectingModel])

	const handleLaunch = async () => {
		const sessionId = await wizardStore.createAndLaunch()
		if (sessionId) {
			wizardStore.reset()
			router.push(`/grading/${sessionId}`)
		}
	}

	return (
		<div className="space-y-6">
			<Card>
				<CardHeader className="pb-2">
					<CardTitle className="text-base">Session Summary</CardTitle>
				</CardHeader>
				<CardContent className="space-y-3 text-sm">
					<div>
						<span className="font-medium">Name: </span>
						<span>{sessionName}</span>
					</div>
					<div>
						<span className="font-medium">Assignment: </span>
						<span className="text-muted-foreground">
							{assignmentText ? `${assignmentText.slice(0, 100)}...` : "Not provided"}
						</span>
					</div>
					<div>
						<span className="font-medium">Model Answer: </span>
						<span className="text-muted-foreground">
							{modelAnswer ? `${modelAnswer.slice(0, 100)}...` : "Not provided"}
						</span>
					</div>
					<div>
						<span className="font-medium">Rubric: </span>
						<span className="text-muted-foreground">
							{rubricParsed
								? `${rubricParsed.criteria.length} criteria, ${rubricParsed.total_max} points`
								: "Not parsed"}
						</span>
					</div>
					<div>
						<span className="font-medium">Submissions: </span>
						<span>{submissionFiles.length} files</span>
					</div>
				</CardContent>
			</Card>

			<div className="space-y-3">
				<Label>Grading Model</Label>
				{isSelectingModel ? (
					<p className="text-sm text-muted-foreground">Selecting optimal model...</p>
				) : (
					<div className="space-y-2">
						{modelReasoning && (
							<p className="text-xs text-muted-foreground p-2 bg-muted rounded-md">
								AI recommendation: {modelReasoning}
							</p>
						)}
						<div className="flex flex-col gap-2">
							{Object.entries(MODEL_LABELS).map(([model, label]) => (
								<button
									key={model}
									type="button"
									className={`text-left px-3 py-2 rounded-md border text-sm transition-colors ${
										selectedModel === model
											? "border-primary bg-primary/5"
											: "border-input hover:bg-muted"
									}`}
									onClick={() => wizardStore.setSelectedModel(model)}
								>
									{label}
									{selectedModel === model && modelReasoning && (
										<span className="text-xs text-muted-foreground ml-2">(recommended)</span>
									)}
								</button>
							))}
						</div>
					</div>
				)}
			</div>

			{isLaunching && launchPhase !== "idle" && (
				<div className="p-3 rounded-md bg-muted space-y-2">
					<p className="text-sm font-medium">
						{PHASE_MESSAGES[launchPhase] || "Processing..."}
					</p>
					<div className="flex gap-1">
						{(["uploading", "creating", "starting"] as const).map((phase) => {
							const phases = ["uploading", "creating", "starting"]
							const currentIdx = phases.indexOf(launchPhase)
							const phaseIdx = phases.indexOf(phase)
							const style =
								phaseIdx < currentIdx
									? "bg-primary"
									: phaseIdx === currentIdx
										? "bg-primary animate-pulse"
										: "bg-muted-foreground/20"
							return (
								<div key={phase} className={`h-1.5 flex-1 rounded-full transition-colors ${style}`} />
							)
						})}
					</div>
				</div>
			)}

			<div className="flex justify-between">
				<Button variant="outline" onClick={() => wizardStore.prevStep()} disabled={isLaunching}>
					Back
				</Button>
				<Button
					onClick={handleLaunch}
					disabled={isLaunching || !selectedModel || !rubricParsed}
				>
					{isLaunching ? "Launching..." : "Start Grading"}
				</Button>
			</div>
		</div>
	)
}
