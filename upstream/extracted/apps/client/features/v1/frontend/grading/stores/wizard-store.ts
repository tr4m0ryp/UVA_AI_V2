import { proxy } from "valtio"
import { handleBulkCreateSubmissions } from "@/features/v1/backend/grading/models/submission-entry"
import { handleUpdateGradingSession } from "@/features/v1/backend/grading/models/grading-session-entry"
import type { StructuredRubric } from "@/features/v1/backend/grading/models/grading-types"
import { showError, showSuccess } from "@/features/v1/frontend/shared/ui/global-message-store"
import { useSnapshot } from "@/features/v1/lib/utils/valtio-hooks"
import { ACCEPTED_FILE_EXTENSIONS } from "../constants"
import { gradingStore } from "./grading-store"

interface SubmissionFile {
	file: File
	name: string
}

export type LaunchPhase = "idle" | "uploading" | "creating" | "starting" | "done"

class WizardState {
	public currentStep: number = 0
	public assignmentText: string = ""
	public assignmentFile: File | null = null
	public modelAnswer: string = ""
	public modelAnswerFile: File | null = null
	public rubricRaw: string = ""
	public rubricFile: File | null = null
	public rubricParsed: StructuredRubric | null = null
	public isParsingRubric: boolean = false
	public submissionFiles: SubmissionFile[] = []
	public selectedModel: string | null = null
	public modelReasoning: string | null = null
	public isSelectingModel: boolean = false
	public sessionName: string = ""
	public isLaunching: boolean = false
	public launchPhase: LaunchPhase = "idle"

	public nextStep() {
		if (this.currentStep < 3) {
			this.currentStep++
		}
	}

	public prevStep() {
		if (this.currentStep > 0) {
			this.currentStep--
		}
	}

	public goToStep(step: number) {
		if (step >= 0 && step <= 3) {
			this.currentStep = step
		}
	}

	public setAssignmentText(text: string) {
		this.assignmentText = text
	}

	public setAssignmentFile(file: File | null) {
		this.assignmentFile = file
	}

	public setModelAnswer(text: string) {
		this.modelAnswer = text
	}

	public setModelAnswerFile(file: File | null) {
		this.modelAnswerFile = file
	}

	public setRubricRaw(text: string) {
		this.rubricRaw = text
	}

	public setRubricFile(file: File | null) {
		this.rubricFile = file
	}

	public setSessionName(name: string) {
		this.sessionName = name
	}

	public addSubmissionFiles(files: File[]) {
		const existingNames = new Set(this.submissionFiles.map((sf) => sf.name))

		const validated = files.filter((f) => {
			const ext = f.name.split(".").pop()?.toLowerCase()
			if (!ext || !ACCEPTED_FILE_EXTENSIONS.includes(ext as "pdf" | "txt")) {
				return false
			}
			if (existingNames.has(f.name)) {
				return false
			}
			existingNames.add(f.name)
			return true
		})

		if (validated.length > 0) {
			const newFiles = validated.map((f) => ({ file: f, name: f.name }))
			this.submissionFiles = [...this.submissionFiles, ...newFiles]
		}
	}

	public removeSubmissionFile(index: number) {
		this.submissionFiles = this.submissionFiles.filter((_, i) => i !== index)
	}

	public clearSubmissionFiles() {
		this.submissionFiles = []
	}

	public async parseRubric(): Promise<boolean> {
		if (!this.rubricRaw && !this.rubricFile) {
			showError("Please provide a rubric to parse")
			return false
		}

		this.isParsingRubric = true
		try {
			let rubricText = this.rubricRaw
			if (this.rubricFile && !rubricText) {
				const ext = this.rubricFile.name.split(".").pop()?.toLowerCase()
				if (ext === "pdf") {
					// PDF rubric: send the file to parse-rubric as FormData would be complex,
					// so we note that the rubric text area must be filled for PDFs
					showError("PDF rubrics must be parsed server-side. Please paste the rubric text or use a .txt file.")
					return false
				}
				rubricText = await this.rubricFile.text()
				this.rubricRaw = rubricText
			}

			const response = await fetch("/api/grading/parse-rubric", {
				method: "POST",
				headers: { "Content-Type": "application/json" },
				body: JSON.stringify({ rubricText }),
			})

			if (!response.ok) {
				const body = await response.json().catch(() => null)
				throw new Error(body?.error || "Failed to parse rubric")
			}

			const parsed = await response.json()
			this.rubricParsed = parsed as StructuredRubric
			showSuccess({ title: "Rubric parsed", description: "Rubric has been structured successfully" })
			return true
		} catch (error) {
			showError(`Error parsing rubric: ${error}`)
			return false
		} finally {
			this.isParsingRubric = false
		}
	}

	public async selectModel(): Promise<boolean> {
		this.isSelectingModel = true
		try {
			const response = await fetch("/api/grading/select-model", {
				method: "POST",
				headers: { "Content-Type": "application/json" },
				body: JSON.stringify({
					assignmentText: this.assignmentText,
					rubricSummary: this.rubricRaw.slice(0, 1000),
				}),
			})

			if (!response.ok) throw new Error("Failed to select model")
			const { model, reasoning } = await response.json()
			this.selectedModel = model
			this.modelReasoning = reasoning
			return true
		} catch (error) {
			showError(`Error selecting model: ${error}`)
			return false
		} finally {
			this.isSelectingModel = false
		}
	}

	public setSelectedModel(model: string) {
		this.selectedModel = model
	}

	public async createAndLaunch(): Promise<string | null> {
		if (!this.sessionName) {
			showError("Please provide a session name")
			return null
		}

		if (this.submissionFiles.length === 0) {
			showError("Please upload at least one submission")
			return null
		}

		this.isLaunching = true
		this.launchPhase = "uploading"
		try {
			const session = await gradingStore.createSession({
				name: this.sessionName,
				assignmentText: this.assignmentText,
				rubricRaw: this.rubricRaw,
				modelAnswer: this.modelAnswer,
			})

			if (!session) throw new Error("Failed to create session")

			if (this.rubricParsed) {
				await handleUpdateGradingSession(session.id, {
					rubricStructured: this.rubricParsed,
					selectedModel: this.selectedModel || undefined,
					modelReasoning: this.modelReasoning || undefined,
				})
			}

			// Upload files to Supabase Storage and extract text server-side
			const formData = new FormData()
			formData.append("sessionId", session.id)
			for (const sf of this.submissionFiles) {
				formData.append("files", sf.file)
			}

			const uploadResponse = await fetch("/api/grading/upload", {
				method: "POST",
				body: formData,
			})

			if (!uploadResponse.ok) {
				const body = await uploadResponse.json().catch(() => null)
				throw new Error(body?.error || "Failed to upload files")
			}

			const uploadResults: Array<{
				studentIdentifier: string
				filePath: string
				contentExtracted: string
			}> = await uploadResponse.json()

			// Create submission records from server-extracted data
			this.launchPhase = "creating"
			await handleBulkCreateSubmissions(session.id, uploadResults)

			await handleUpdateGradingSession(session.id, {
				totalSubmissions: uploadResults.length,
			})

			// Trigger the grading pipeline
			this.launchPhase = "starting"
			const runResponse = await fetch("/api/grading/run", {
				method: "POST",
				headers: { "Content-Type": "application/json" },
				body: JSON.stringify({ sessionId: session.id }),
			})

			if (!runResponse.ok) {
				const body = await runResponse.json().catch(() => null)
				showError(body?.error || "Failed to start grading pipeline")
				// Session is created but pipeline failed -- user can retry from detail page
			}

			this.launchPhase = "done"
			showSuccess({
				title: "Grading started",
				description: `Grading ${uploadResults.length} submissions`,
			})
			return session.id
		} catch (error) {
			showError(`Error launching grading: ${error}`)
			return null
		} finally {
			this.isLaunching = false
			this.launchPhase = "idle"
		}
	}

	public reset() {
		this.currentStep = 0
		this.assignmentText = ""
		this.assignmentFile = null
		this.modelAnswer = ""
		this.modelAnswerFile = null
		this.rubricRaw = ""
		this.rubricFile = null
		this.rubricParsed = null
		this.isParsingRubric = false
		this.submissionFiles = []
		this.selectedModel = null
		this.modelReasoning = null
		this.isSelectingModel = false
		this.sessionName = ""
		this.isLaunching = false
		this.launchPhase = "idle"
	}
}

export const useWizardState = () => {
	return useSnapshot(wizardStore)
}

export const wizardStore = proxy(new WizardState())
