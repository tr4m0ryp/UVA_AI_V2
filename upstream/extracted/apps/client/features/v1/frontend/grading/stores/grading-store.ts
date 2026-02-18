import { proxy } from "valtio"
import {
	handleGetAllGradingSessions,
	handleGetGradingSession,
	handleCreateGradingSession,
	handleUpdateGradingSession,
	handleDeleteGradingSession,
} from "@/features/v1/backend/grading/models/grading-session-entry"
import {
	handleGetSubmissions,
	handleGetSubmission,
} from "@/features/v1/backend/grading/models/submission-entry"
import type { GradingSession, Submission } from "@/features/v1/backend/grading/models/grading-types"
import { showError } from "@/features/v1/frontend/shared/ui/global-message-store"
import { useSnapshot } from "@/features/v1/lib/utils/valtio-hooks"

class GradingState {
	public sessions: GradingSession[] = []
	public currentSession: GradingSession | null = null
	public submissions: Submission[] = []
	public selectedSubmission: Submission | null = null
	public isLoading: boolean = false
	public errors: string[] = []
	public pendingDeleteId: string | null = null
	private pollingInterval: ReturnType<typeof setInterval> | null = null

	public setLoading(isLoading: boolean) {
		this.isLoading = isLoading
	}

	public updateErrors(errors: string[]) {
		this.errors = errors
	}

	public async loadSessions() {
		this.setLoading(true)
		try {
			const sessions = await handleGetAllGradingSessions()
			this.sessions = sessions
		} catch (error) {
			showError(`Error loading grading sessions: ${error}`)
		} finally {
			this.setLoading(false)
		}
	}

	public async loadSession(sessionId: string) {
		this.setLoading(true)
		try {
			const [session, submissions] = await Promise.all([
				handleGetGradingSession(sessionId),
				handleGetSubmissions(sessionId),
			])
			this.currentSession = session
			this.submissions = submissions
		} catch (error) {
			showError(`Error loading session: ${error}`)
		} finally {
			this.setLoading(false)
		}
	}

	public async loadSubmission(submissionId: string) {
		try {
			const submission = await handleGetSubmission(submissionId)
			this.selectedSubmission = submission
		} catch (error) {
			showError(`Error loading submission: ${error}`)
		}
	}

	public async createSession(input: {
		name: string
		assignmentText?: string
		rubricRaw?: string
		modelAnswer?: string
	}): Promise<GradingSession | null> {
		this.setLoading(true)
		const originalSessions = [...this.sessions]

		try {
			const tempId = `temp_${Date.now()}`
			const tempSession = {
				id: tempId,
				name: input.name,
				status: "draft" as const,
				totalSubmissions: 0,
				gradedCount: 0,
				createdAt: new Date(),
				updatedAt: new Date(),
			} as GradingSession
			this.sessions = [tempSession, ...this.sessions]

			const session = await handleCreateGradingSession(input)
			this.sessions = this.sessions.map((s) => (s.id === tempId ? session : s))
			return session
		} catch (error) {
			this.sessions = originalSessions
			showError(`Error creating session: ${error}`)
			return null
		} finally {
			this.setLoading(false)
		}
	}

	public async updateSession(
		sessionId: string,
		updates: Parameters<typeof handleUpdateGradingSession>[1],
	): Promise<GradingSession | null> {
		this.setLoading(true)
		const originalSession = this.currentSession
		const originalSessions = [...this.sessions]

		try {
			if (this.currentSession?.id === sessionId) {
				this.currentSession = { ...this.currentSession, ...updates } as GradingSession
			}

			const updated = await handleUpdateGradingSession(sessionId, updates)

			this.currentSession = updated
			this.sessions = this.sessions.map((s) => (s.id === sessionId ? updated : s))
			return updated
		} catch (error) {
			this.currentSession = originalSession
			this.sessions = originalSessions
			showError(`Error updating session: ${error}`)
			return null
		} finally {
			this.setLoading(false)
		}
	}

	public confirmDelete(sessionId: string) {
		this.pendingDeleteId = sessionId
	}

	public cancelDelete() {
		this.pendingDeleteId = null
	}

	public async executeDelete(): Promise<boolean> {
		if (!this.pendingDeleteId) return false

		const sessionId = this.pendingDeleteId
		this.pendingDeleteId = null
		return this.deleteSession(sessionId)
	}

	public async deleteSession(sessionId: string): Promise<boolean> {
		this.setLoading(true)
		const originalSessions = [...this.sessions]

		try {
			this.sessions = this.sessions.filter((s) => s.id !== sessionId)
			await handleDeleteGradingSession(sessionId)

			if (this.currentSession?.id === sessionId) {
				this.currentSession = null
				this.submissions = []
			}
			return true
		} catch (error) {
			this.sessions = originalSessions
			showError(`Error deleting session: ${error}`)
			return false
		} finally {
			this.setLoading(false)
		}
	}

	public startPolling(sessionId: string) {
		this.stopPolling()

		this.pollingInterval = setInterval(async () => {
			try {
				const [session, submissions] = await Promise.all([
					handleGetGradingSession(sessionId),
					handleGetSubmissions(sessionId),
				])

				this.currentSession = session
				this.submissions = submissions

				this.sessions = this.sessions.map((s) =>
					s.id === sessionId ? session : s,
				)

				if (session.status === "completed" || session.status === "failed") {
					this.stopPolling()
				}
			} catch {
				// Polling failure is non-fatal
			}
		}, 3000)
	}

	public stopPolling() {
		if (this.pollingInterval) {
			clearInterval(this.pollingInterval)
			this.pollingInterval = null
		}
	}
}

export const useGradingState = () => {
	return useSnapshot(gradingStore)
}

export const gradingStore = proxy(new GradingState())

export const initializeGradingData = async () => {
	gradingStore.setLoading(true)
	try {
		const sessions = await handleGetAllGradingSessions()
		gradingStore.sessions = sessions
	} catch (error) {
		showError(`Error initializing grading data: ${error}`)
	} finally {
		gradingStore.setLoading(false)
	}
}
