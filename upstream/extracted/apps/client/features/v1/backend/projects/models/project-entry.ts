import type { FrontEndProjectModel, FrontEndRelationshipModel } from "./models"
import type { FrontEndChatThreadModel } from "@/features/v1/backend/chat/new/models"
import type { FrontEndPersonaModel } from "@/features/v1/backend/personas/models/models"
import type { FrontEndPromptModel } from "@/features/v1/backend/prompts/models/models"

export async function handleAddChatToProject(_input: { parentId: string; childId: string }): Promise<FrontEndRelationshipModel> { return {} as FrontEndRelationshipModel }
export async function handleAddPersonaToProject(_input: { parentId: string; childId: string }): Promise<FrontEndRelationshipModel> { return {} as FrontEndRelationshipModel }
export async function handleAddPromptToProject(_input: { parentId: string; childId: string }): Promise<FrontEndRelationshipModel> { return {} as FrontEndRelationshipModel }
export async function handleCreateProject(_input: unknown): Promise<FrontEndProjectModel> { return {} as FrontEndProjectModel }
export async function handleDeleteProject(_projectId: string): Promise<boolean> { return true }
export async function handleGetAllProjectsAndContents(): Promise<{
	projects: FrontEndProjectModel[]
	relationships: FrontEndRelationshipModel[]
	chats: FrontEndChatThreadModel[]
	personas: FrontEndPersonaModel[]
	prompts: FrontEndPromptModel[]
}> {
	return { projects: [], relationships: [], chats: [], personas: [], prompts: [] }
}
export async function handleRemoveFromProject(_input: { parentId: string; childId: string }): Promise<boolean> { return true }
export async function handleRemoveProjectFromParent(_projectId: string): Promise<FrontEndProjectModel> { return {} as FrontEndProjectModel }
export async function handleUpdateProject(_project: FrontEndProjectModel): Promise<FrontEndProjectModel> { return {} as FrontEndProjectModel }
