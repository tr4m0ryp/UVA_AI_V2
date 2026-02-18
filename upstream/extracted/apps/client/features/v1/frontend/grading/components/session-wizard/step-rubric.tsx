"use client"

import { Input } from "@/features/v1/frontend/shared/ui/input"
import { Label } from "@/features/v1/frontend/shared/ui/label"
import { Textarea } from "@/features/v1/frontend/shared/ui/textarea"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import { useWizardState, wizardStore } from "../../stores/wizard-store"
import { RubricViewer } from "../rubric/rubric-viewer"

function isPdf(filename: string): boolean {
	return filename.toLowerCase().endsWith(".pdf")
}

export const StepRubric = () => {
	const { rubricRaw, rubricFile, rubricParsed, isParsingRubric } = useWizardState()

	return (
		<div className="space-y-6">
			<div className="space-y-2">
				<Label htmlFor="rubric-text">Grading Rubric</Label>
				<Textarea
					id="rubric-text"
					placeholder="Paste your rubric here. Include criteria names, point ranges, and level descriptions..."
					className="min-h-[200px]"
					value={rubricRaw}
					onChange={(e) => wizardStore.setRubricRaw(e.target.value)}
				/>
				<div className="flex items-center gap-2">
					<span className="text-xs text-muted-foreground">Or upload:</span>
					<Input
						type="file"
						accept=".pdf,.txt"
						className="h-8 text-xs w-auto"
						onChange={(e) => {
							const file = e.target.files?.[0] || null
							wizardStore.setRubricFile(file)
							if (file) {
								if (isPdf(file.name)) {
									// PDF content cannot be read client-side; user must paste text
									wizardStore.setRubricRaw("")
								} else {
									file.text().then((text) => wizardStore.setRubricRaw(text))
								}
							}
						}}
					/>
					{rubricFile && (
						<span className="text-xs text-muted-foreground">
							{rubricFile.name}
							{isPdf(rubricFile.name) && (
								<span className="ml-1 text-amber-600 dark:text-amber-400">
									(PDF detected -- please paste rubric text above for parsing)
								</span>
							)}
						</span>
					)}
				</div>
			</div>

			<div className="flex items-center gap-3">
				<Button
					variant="secondary"
					onClick={() => wizardStore.parseRubric()}
					disabled={isParsingRubric || (!rubricRaw && !rubricFile)}
				>
					{isParsingRubric ? "Parsing..." : "Parse Rubric"}
				</Button>
				{rubricParsed && (
					<span className="text-sm text-green-600 dark:text-green-400">
						Parsed: {rubricParsed.criteria.length} criteria, {rubricParsed.total_max} total points
					</span>
				)}
			</div>

			{rubricParsed && (
				<div className="border rounded-lg p-4">
					<h3 className="text-sm font-medium mb-3">Parsed Rubric Preview</h3>
					<RubricViewer rubric={rubricParsed} />
				</div>
			)}

			<div className="flex justify-between">
				<Button variant="outline" onClick={() => wizardStore.prevStep()}>
					Back
				</Button>
				<Button
					onClick={() => wizardStore.nextStep()}
					disabled={!rubricParsed}
				>
					Next: Submissions
				</Button>
			</div>
		</div>
	)
}
