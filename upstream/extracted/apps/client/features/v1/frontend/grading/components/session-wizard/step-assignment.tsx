"use client"

import { Input } from "@/features/v1/frontend/shared/ui/input"
import { Label } from "@/features/v1/frontend/shared/ui/label"
import { Textarea } from "@/features/v1/frontend/shared/ui/textarea"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import { useWizardState, wizardStore } from "../../stores/wizard-store"

function isPdf(filename: string): boolean {
	return filename.toLowerCase().endsWith(".pdf")
}

export const StepAssignment = () => {
	const { sessionName, assignmentText, modelAnswer, assignmentFile, modelAnswerFile } =
		useWizardState()

	return (
		<div className="space-y-6">
			<div className="space-y-2">
				<Label htmlFor="session-name">Session Name</Label>
				<Input
					id="session-name"
					placeholder="e.g., Midterm Exam 2026 - Group A"
					value={sessionName}
					onChange={(e) => wizardStore.setSessionName(e.target.value)}
				/>
			</div>

			<div className="space-y-2">
				<Label htmlFor="assignment-text">Assignment Description</Label>
				<Textarea
					id="assignment-text"
					placeholder="Paste the assignment text or instructions here..."
					className="min-h-[150px]"
					value={assignmentText}
					onChange={(e) => wizardStore.setAssignmentText(e.target.value)}
				/>
				<div className="flex items-center gap-2">
					<span className="text-xs text-muted-foreground">Or upload:</span>
					<Input
						type="file"
						accept=".pdf,.txt"
						className="h-8 text-xs w-auto"
						onChange={(e) => {
							const file = e.target.files?.[0] || null
							wizardStore.setAssignmentFile(file)
							if (file) {
								if (isPdf(file.name)) {
									// PDF content will be extracted server-side
									wizardStore.setAssignmentText("")
								} else {
									file.text().then((text) => wizardStore.setAssignmentText(text))
								}
							}
						}}
					/>
					{assignmentFile && (
						<span className="text-xs text-muted-foreground">
							{assignmentFile.name}
							{isPdf(assignmentFile.name) && (
								<span className="ml-1 text-blue-600 dark:text-blue-400">
									(PDF -- text extracted server-side)
								</span>
							)}
						</span>
					)}
				</div>
			</div>

			<div className="space-y-2">
				<Label htmlFor="model-answer">Model Answer (optional)</Label>
				<Textarea
					id="model-answer"
					placeholder="Paste the ideal/model answer here..."
					className="min-h-[150px]"
					value={modelAnswer}
					onChange={(e) => wizardStore.setModelAnswer(e.target.value)}
				/>
				<div className="flex items-center gap-2">
					<span className="text-xs text-muted-foreground">Or upload:</span>
					<Input
						type="file"
						accept=".pdf,.txt"
						className="h-8 text-xs w-auto"
						onChange={(e) => {
							const file = e.target.files?.[0] || null
							wizardStore.setModelAnswerFile(file)
							if (file) {
								if (isPdf(file.name)) {
									wizardStore.setModelAnswer("")
								} else {
									file.text().then((text) => wizardStore.setModelAnswer(text))
								}
							}
						}}
					/>
					{modelAnswerFile && (
						<span className="text-xs text-muted-foreground">
							{modelAnswerFile.name}
							{isPdf(modelAnswerFile.name) && (
								<span className="ml-1 text-blue-600 dark:text-blue-400">
									(PDF -- text extracted server-side)
								</span>
							)}
						</span>
					)}
				</div>
			</div>

			<div className="flex justify-end">
				<Button
					onClick={() => wizardStore.nextStep()}
					disabled={!sessionName.trim()}
				>
					Next: Rubric
				</Button>
			</div>
		</div>
	)
}
