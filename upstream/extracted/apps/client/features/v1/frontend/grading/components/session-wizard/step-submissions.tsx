"use client"

import { useCallback, useMemo } from "react"
import { Input } from "@/features/v1/frontend/shared/ui/input"
import { Label } from "@/features/v1/frontend/shared/ui/label"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import { ScrollArea } from "@/features/v1/frontend/shared/ui/scroll-area"
import { useWizardState, wizardStore } from "../../stores/wizard-store"
import { ACCEPTED_FILE_EXTENSIONS } from "../../constants"

function formatFileSize(bytes: number): string {
	if (bytes < 1024) return `${bytes} B`
	if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
	return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
}

function isValidExtension(filename: string): boolean {
	const ext = filename.split(".").pop()?.toLowerCase()
	return !!ext && ACCEPTED_FILE_EXTENSIONS.includes(ext as "pdf" | "txt")
}

export const StepSubmissions = () => {
	const { submissionFiles } = useWizardState()

	const handleFileChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
		const files = e.target.files
		if (!files) return
		const valid = Array.from(files).filter((f) => isValidExtension(f.name))
		if (valid.length > 0) {
			wizardStore.addSubmissionFiles(valid)
		}
		e.target.value = ""
	}, [])

	const handleDrop = useCallback((e: React.DragEvent) => {
		e.preventDefault()
		const files = Array.from(e.dataTransfer.files).filter((f) => isValidExtension(f.name))
		if (files.length > 0) {
			wizardStore.addSubmissionFiles(files)
		}
	}, [])

	const handleDragOver = useCallback((e: React.DragEvent) => {
		e.preventDefault()
	}, [])

	const totalSize = useMemo(
		() => submissionFiles.reduce((sum, sf) => sum + sf.file.size, 0),
		[submissionFiles],
	)

	return (
		<div className="space-y-6">
			<div className="space-y-2">
				<Label>Student Submissions</Label>
				<p className="text-xs text-muted-foreground">
					Upload PDF or TXT files. Student ID is parsed from filename: StudentID_AssignmentName.pdf
				</p>
			</div>

			<div
				className="border-2 border-dashed rounded-lg p-8 text-center cursor-pointer hover:border-primary/50 transition-colors"
				onDrop={handleDrop}
				onDragOver={handleDragOver}
				onClick={() => document.getElementById("submission-files-input")?.click()}
			>
				<p className="text-sm text-muted-foreground mb-2">
					Drag and drop files here, or click to browse
				</p>
				<p className="text-xs text-muted-foreground">
					Accepted formats: .pdf, .txt
				</p>
				<Input
					id="submission-files-input"
					type="file"
					accept=".pdf,.txt"
					multiple
					className="hidden"
					onChange={handleFileChange}
				/>
			</div>

			{submissionFiles.length > 0 && (
				<div className="space-y-2">
					<div className="flex items-center justify-between">
						<span className="text-sm font-medium">
							{submissionFiles.length} file{submissionFiles.length !== 1 ? "s" : ""} selected
							<span className="ml-2 text-muted-foreground font-normal">
								({formatFileSize(totalSize)} total)
							</span>
						</span>
						<Button
							variant="ghost"
							size="sm"
							className="text-xs text-destructive"
							onClick={() => wizardStore.clearSubmissionFiles()}
						>
							Clear all
						</Button>
					</div>
					<ScrollArea className="h-[200px]">
						<div className="space-y-1">
							{submissionFiles.map((sf, index) => (
								<div
									key={`${sf.name}-${index}`}
									className="flex items-center justify-between px-3 py-1.5 rounded-md hover:bg-muted text-sm"
								>
									<div className="flex items-center gap-2 truncate">
										<span className="truncate">{sf.name}</span>
										<span className="text-xs text-muted-foreground flex-shrink-0">
											{formatFileSize(sf.file.size)}
										</span>
									</div>
									<Button
										variant="ghost"
										size="sm"
										className="h-6 text-xs text-muted-foreground hover:text-destructive"
										onClick={() => wizardStore.removeSubmissionFile(index)}
									>
										Remove
									</Button>
								</div>
							))}
						</div>
					</ScrollArea>
				</div>
			)}

			<div className="flex justify-between">
				<Button variant="outline" onClick={() => wizardStore.prevStep()}>
					Back
				</Button>
				<Button
					onClick={() => wizardStore.nextStep()}
					disabled={submissionFiles.length === 0}
				>
					Next: Review
				</Button>
			</div>
		</div>
	)
}
