import { proxy } from "valtio"
import { handleCreateProjectChatThread } from "@/features/v1/backend/chat/models/chat-thread-entry"
import type { FrontEndChatThreadModel } from "@/features/v1/backend/chat/new/models"
import type { FrontEndPersonaModel } from "@/features/v1/backend/personas/models/models"
import type {
	FrontEndProjectModel,
	FrontEndRelationshipModel,
} from "@/features/v1/backend/projects/models/models"
import {
	handleAddChatToProject,
	handleAddPersonaToProject,
	handleAddPromptToProject,
	handleCreateProject,
	handleDeleteProject,
	handleGetAllProjectsAndContents,
	handleRemoveFromProject,
	handleRemoveProjectFromParent,
	handleUpdateProject,
} from "@/features/v1/backend/projects/models/project-entry"
import type { ProjectInput } from "@/features/v1/backend/projects/services/project-service"
import type { FrontEndPromptModel } from "@/features/v1/backend/prompts/models/models"
import { showError } from "@/features/v1/frontend/shared/ui/global-message-store"
import { useSnapshot } from "@/features/v1/lib/utils/valtio-hooks"

class ProjectState {
	public errors: string[] = []
	public currentProjectId: string | null = null
	public isLoading: boolean = false
	public projects: FrontEndProjectModel[] = []
	public relationships: FrontEndRelationshipModel[] = []
	public chats: FrontEndChatThreadModel[] = []
	public personas: FrontEndPersonaModel[] = []
	public prompts: FrontEndPromptModel[] = []
	public isInfoOpened: boolean = false

	public openInfo() {
		this.isInfoOpened = true
	}

	public closeInfo() {
		this.isInfoOpened = false
	}

	public updateErrors(errors: string[]) {
		this.errors = errors
	}

	public setCurrentProjectId(projectId: string | null) {
		this.currentProjectId = projectId
	}

	public setLoading(isLoading: boolean) {
		this.isLoading = isLoading
	}

	/**
	 * Sets all project-related data from a given data object.
	 * @param data - An object containing arrays of projects, relationships, chats, personas, and prompts.
	 */
	public setProjectData(data: {
		projects: FrontEndProjectModel[]
		relationships: FrontEndRelationshipModel[]
		chats: FrontEndChatThreadModel[]
		personas: FrontEndPersonaModel[]
		prompts: FrontEndPromptModel[]
	}) {
		this.projects = data.projects
		this.relationships = data.relationships
		this.chats = data.chats
		this.personas = data.personas
		this.prompts = data.prompts
	}

	/**
	 * Converts FormData to a FrontEndProjectModel.
	 * @param formData - The FormData object to convert.
	 * @returns A FrontEndProjectModel object.
	 */
	private formDataToProjectModel(formData: FormData): FrontEndProjectModel {
		const color = formData.get("color") as string | null

		return {
			id: formData.get("id") as string,
			name: formData.get("name") as string,
			parentId: formData.get("parentId") as string | null,
			createdAt: new Date(),
			color: color ? color : null,
			customInstructions: formData.get("customInstructions") as string | null,
		}
	}

	/**
	 * Creates a new project or updates an existing one based on FormData.
	 * It handles optimistic updates to the UI.
	 * @param formData - The FormData containing project data.
	 * @returns The created or updated project model.
	 */
	public async createOrUpdateProject(formData: FormData) {
		const model = this.formDataToProjectModel(formData)
		this.updateErrors([])
		this.setLoading(true)

		// Save original data for potential rollback
		const originalProjects = [...this.projects]

		try {
			let response: FrontEndProjectModel

			if (model.id && model.id !== "") {
				// Optimistically update the project in store
				const updatedProjects = this.projects.map((p) =>
					p.id === model.id ? { ...p, ...model } : p,
				)
				this.projects = updatedProjects

				const projectToUpsert: FrontEndProjectModel = {
					id: model.id,
					name: model.name,
					color: model.color || null,
					createdAt: model.createdAt,
					parentId: model.parentId,
					customInstructions: model.customInstructions,
				}

				response = await handleUpdateProject(projectToUpsert)

				// Update with the actual response
				this.projects = this.projects.map((p) => (p.id === response.id ? response : p))
			} else {
				// Create temp project for optimistic update
				const tempId = `temp_${Date.now()}`
				const tempProject = { ...model, id: tempId }
				this.projects = [tempProject, ...this.projects]

				const createProps: ProjectInput = {
					name: model.name,
					color: model.color || null,
					customInstructions: model.customInstructions,
				}

				if (!model.id && this.currentProjectId) {
					createProps.parentId = this.currentProjectId
				}

				response = await handleCreateProject(createProps)

				// Replace temp project with actual response
				this.projects = this.projects.map((p) => (p.id === tempId ? response : p))
			}

			return response
		} catch (error) {
			this.projects = originalProjects
			this.updateErrors([error instanceof Error ? error.message : "An error occurred"])
			throw error
		} finally {
			this.setLoading(false)
		}
	}

	/**
	 * Deletes a project by its ID.
	 * It handles optimistic updates to the UI.
	 * @param projectId - The ID of the project to delete.
	 * @returns A boolean indicating success.
	 */
	public async deleteProject(projectId: string) {
		this.setLoading(true)

		// Save original data for potential rollback
		const originalProjects = [...this.projects]
		const originalRelationships = [...this.relationships]

		try {
			const projectToDelete = this.projects.find((p) => p.id === projectId)
			if (!projectToDelete) {
				throw new Error("Project not found")
			}

			// Optimistically remove the project and its relationships
			this.projects = this.projects.filter((p) => p.id !== projectId)
			this.relationships = this.relationships.filter(
				(r) => r.parentId !== projectId && r.childId !== projectId,
			)

			const success = await handleDeleteProject(projectId)

			if (!success) {
				throw new Error("Failed to delete project")
			}

			return success
		} catch (error) {
			this.projects = originalProjects
			this.relationships = originalRelationships
			this.updateErrors([error instanceof Error ? error.message : "An error occurred"])
			throw error
		} finally {
			this.setLoading(false)
		}
	}

	/**
	 * Adds a chat to the current project.
	 * It handles optimistic updates to the UI.
	 * @param chatId - The ID of the chat to add.
	 * @returns The newly created relationship model.
	 */
	public async addChatToProject(chatId: string) {
		if (!this.currentProjectId) {
			return
		}

		this.setLoading(true)
		const originalRelationships = [...this.relationships]

		try {
			// Create temp relationship for optimistic update
			const tempRelationship: FrontEndRelationshipModel = {
				id: `temp_${Date.now()}`,
				parentId: this.currentProjectId,
				childId: chatId,
				childType: "chat",
			}

			// Optimistically add the relationship
			this.relationships = [...this.relationships, tempRelationship]

			const response = await handleAddChatToProject({
				parentId: this.currentProjectId,
				childId: chatId,
			})

			// Replace temp relationship with actual response
			this.relationships = this.relationships.map((r) =>
				r.id === tempRelationship.id ? response : r,
			)

			return response
		} catch (error) {
			this.relationships = originalRelationships
			this.updateErrors([error instanceof Error ? error.message : "An error occurred"])
			throw error
		} finally {
			this.setLoading(false)
		}
	}

	/**
	 * Removes a chat from the current project.
	 * It handles optimistic updates to the UI.
	 * @param chatId - The ID of the chat to remove.
	 * @returns A boolean indicating success.
	 */
	public async removeChatFromProject(chatId: string): Promise<boolean> {
		if (!this.currentProjectId) {
			return false
		}

		this.setLoading(true)
		const originalRelationships = [...this.relationships]

		try {
			const relationshipToRemove = this.relationships.find(
				(r) =>
					r.parentId === this.currentProjectId && r.childId === chatId && r.childType === "chat",
			)

			if (!relationshipToRemove) {
				throw new Error("Relationship not found")
			}

			// Optimistically remove the relationship
			this.relationships = this.relationships.filter(
				(r) =>
					!(r.parentId === this.currentProjectId && r.childId === chatId && r.childType === "chat"),
			)

			const success = await handleRemoveFromProject({
				parentId: this.currentProjectId,
				childId: chatId,
			})

			if (!success) {
				throw new Error("Failed to remove chat from project")
			}

			return success
		} catch (error) {
			this.relationships = originalRelationships
			this.updateErrors([error instanceof Error ? error.message : "An error occurred"])
			throw error
		} finally {
			this.setLoading(false)
		}
	}

	/**
	 * Edits the name of a project.
	 * @param project - The project model to edit.
	 * @param newName - The new name for the project.
	 * @returns The updated project model.
	 */
	public async editProjectName(project: FrontEndProjectModel, newName: string) {
		const formData = new FormData()
		formData.append("id", project.id)
		formData.append("name", newName)
		formData.append("createdAt", project.createdAt.toISOString())
		formData.append("parentId", project.parentId || "")
		if (project.color) {
			formData.append("color", project.color)
		}
		if (project.customInstructions) {
			formData.append("customInstructions", project.customInstructions)
		}

		return await this.createOrUpdateProject(formData)
	}

	/**
	 * Updates the color of a project.
	 * @param project - The project model to update.
	 * @param color - The new color for the project.
	 * @returns The updated project model.
	 */
	public async updateProjectColor(project: FrontEndProjectModel, color: string) {
		const formData = new FormData()
		formData.append("id", project.id)
		formData.append("name", project.name)
		formData.append("createdAt", project.createdAt.toISOString())
		formData.append("parentId", project.parentId || "")
		formData.append("color", color)
		if (project.customInstructions) {
			formData.append("customInstructions", project.customInstructions)
		}

		return await this.createOrUpdateProject(formData)
	}

	/**
	 * Creates a new chat thread associated with the current project.
	 * @returns The ID of the newly created chat thread.
	 */
	public async createProjectChat(): Promise<string | undefined> {
		if (!this.currentProjectId) {
			return
		}

		try {
			const chatId = await handleCreateProjectChatThread(this.currentProjectId)
			return chatId
		} catch (error) {
			this.updateErrors([
				error instanceof Error ? error.message : "An error occurred creating chat",
			])
			throw error
		}
	}

	/**
	 * Adds an existing project to another project as a sub-directory.
	 * @param project - The project to move.
	 * @param parentProjectId - The ID of the new parent project.
	 * @returns The updated project model.
	 */
	public async addProjectToProject(project: FrontEndProjectModel, parentProjectId: string) {
		const formData = new FormData()
		formData.append("id", project.id)
		formData.append("name", project.name)
		formData.append("createdAt", project.createdAt.toISOString())
		formData.append("parentId", parentProjectId)

		if (project.color) {
			formData.append("color", project.color)
		}
		if (project.customInstructions) {
			formData.append("customInstructions", project.customInstructions)
		}

		return await this.createOrUpdateProject(formData)
	}

	/**
	 * Removes a project from its parent project, moving it up one level in the hierarchy.
	 * @param projectId - The ID of the project to remove from its parent.
	 * @returns The updated project model.
	 */
	public async removeProjectFromParent(projectId: string): Promise<FrontEndProjectModel> {
		this.setLoading(true)
		const originalProjects = [...this.projects]
		const originalRelationships = [...this.relationships]

		try {
			const project = this.projects.find((p) => p.id === projectId)
			if (!project) {
				throw new Error("Project not found")
			}

			if (!project.parentId) {
				throw new Error("Project is not nested in a parent project")
			}

			const parentId = project.parentId
			const parentProject = this.projects.find((p) => p.id === parentId)
			if (!parentProject) {
				throw new Error("Parent project not found")
			}

			// get the parent's parent (new parent)
			const newParentId = parentProject.parentId

			// optimistically update the project to move it one level up
			const updatedProject: FrontEndProjectModel = {
				...project,
				parentId: newParentId,
			}
			this.projects = this.projects.map((p) => (p.id === projectId ? updatedProject : p))

			// remove the relationship from the old parent project
			this.relationships = this.relationships.filter(
				(r) =>
					!(
						r.parentId === parentId &&
						r.childId === projectId &&
						r.childType === "directory"
					),
			)

			// add new relationship to the new parent project
			if (newParentId) {
				const newRelationship: FrontEndRelationshipModel = {
					id: `temp-${Date.now()}`,
					parentId: newParentId,
					childId: projectId,
					childType: "directory",
				}
				this.relationships = [...this.relationships, newRelationship]
			}

			const result = await handleRemoveProjectFromParent(projectId)
			this.projects = this.projects.map((p) => (p.id === projectId ? result : p))

			return result
		} catch (error) {
			this.projects = originalProjects
			this.relationships = originalRelationships
			this.updateErrors([error instanceof Error ? error.message : "An error occurred"])
			throw error
		} finally {
			this.setLoading(false)
		}
	}

	/**
	 * Adds a persona to the current project.
	 * It handles optimistic updates to the UI.
	 * @param personaId - The ID of the persona to add.
	 * @returns The newly created relationship model.
	 */
	public async addPersonaToProject(personaId: string) {
		if (!this.currentProjectId) return

		this.setLoading(true)
		const originalRelationships = [...this.relationships]

		try {
			// Create temp relationship for optimistic update
			const tempRelationship: FrontEndRelationshipModel = {
				id: `temp_${Date.now()}`,
				parentId: this.currentProjectId,
				childId: personaId,
				childType: "persona",
			}

			// Optimistically add the relationship
			this.relationships = [...this.relationships, tempRelationship]

			const response = await handleAddPersonaToProject({
				parentId: this.currentProjectId,
				childId: personaId,
			})

			// Replace temp relationship with actual response
			this.relationships = this.relationships.map((r) =>
				r.id === tempRelationship.id ? response : r,
			)

			return response
		} catch (error) {
			this.relationships = originalRelationships
			this.updateErrors([error instanceof Error ? error.message : "An error occurred"])
			throw error
		} finally {
			this.setLoading(false)
		}
	}

	/**
	 * Adds a prompt to the current project.
	 * It handles optimistic updates to the UI.
	 * @param promptId - The ID of the prompt to add.
	 * @returns The newly created relationship model.
	 */
	public async addPromptToProject(promptId: string) {
		if (!this.currentProjectId) return

		this.setLoading(true)
		const originalRelationships = [...this.relationships]

		try {
			// Create temp relationship for optimistic update
			const tempRelationship: FrontEndRelationshipModel = {
				id: `temp_${Date.now()}`,
				parentId: this.currentProjectId,
				childId: promptId,
				childType: "prompt",
			}

			// Optimistically add the relationship
			this.relationships = [...this.relationships, tempRelationship]

			const response = await handleAddPromptToProject({
				parentId: this.currentProjectId,
				childId: promptId,
			})

			// Replace temp relationship with actual response
			this.relationships = this.relationships.map((r) =>
				r.id === tempRelationship.id ? response : r,
			)

			return response
		} catch (error) {
			// Rollback on error
			this.relationships = originalRelationships
			this.updateErrors([error instanceof Error ? error.message : "An error occurred"])
			throw error
		} finally {
			this.setLoading(false)
		}
	}

	/**
	 * Removes a persona from the current project.
	 * It handles optimistic updates to the UI.
	 * @param personaId - The ID of the persona to remove.
	 * @returns A boolean indicating success.
	 */
	public async removePersonaFromProject(personaId: string): Promise<boolean> {
		if (!this.currentProjectId) return false

		this.setLoading(true)
		const originalRelationships = [...this.relationships]

		try {
			// Optimistically remove the relationship
			this.relationships = this.relationships.filter(
				(r) =>
					!(
						r.parentId === this.currentProjectId &&
						r.childId === personaId &&
						r.childType === "persona"
					),
			)

			const success = await handleRemoveFromProject({
				parentId: this.currentProjectId,
				childId: personaId,
			})

			if (!success) {
				return false
			}

			return success
		} catch (error) {
			this.relationships = originalRelationships
			this.updateErrors([error instanceof Error ? error.message : "An error occurred"])
			throw error
		} finally {
			this.setLoading(false)
		}
	}

	/**
	 * Removes a prompt from the current project.
	 * It handles optimistic updates to the UI.
	 * @param promptId - The ID of the prompt to remove.
	 * @returns A boolean indicating success.
	 */
	public async removePromptFromProject(promptId: string): Promise<boolean> {
		if (!this.currentProjectId) return false

		this.setLoading(true)
		const originalRelationships = [...this.relationships]

		try {
			// Optimistically remove the relationship
			this.relationships = this.relationships.filter(
				(r) =>
					!(
						r.parentId === this.currentProjectId &&
						r.childId === promptId &&
						r.childType === "prompt"
					),
			)

			const success = await handleRemoveFromProject({
				parentId: this.currentProjectId,
				childId: promptId,
			})

			if (!success) {
				return false
			}

			return success
		} catch (error) {
			// Rollback on error
			this.relationships = originalRelationships
			this.updateErrors([error instanceof Error ? error.message : "An error occurred"])
			throw error
		} finally {
			this.setLoading(false)
		}
	}

	/**
	 * Updates the custom instructions for a project.
	 * @param project - The project to update.
	 * @param instructions - The new custom instructions.
	 * @returns The updated project model.
	 */
	public async updateProjectInstructions(project: FrontEndProjectModel, instructions: string) {
		const formData = new FormData()
		formData.append("id", project.id)
		formData.append("name", project.name)
		formData.append("createdAt", project.createdAt.toISOString())
		formData.append("parentId", project.parentId || "")
		formData.append("customInstructions", instructions)
		if (project.color) {
			formData.append("color", project.color)
		}

		return await this.createOrUpdateProject(formData)
	}
}

export const useProjectState = () => {
	return useSnapshot(projectStore)
}

export const projectStore = proxy(new ProjectState())

export const initializeProjectData = async () => {
	projectStore.setLoading(true)

	try {
		const data = await handleGetAllProjectsAndContents()
		projectStore.setProjectData(data)
	} catch (error) {
		showError(`Error initializing projects: ${error}`)
	} finally {
		projectStore.setLoading(false)
	}
}
