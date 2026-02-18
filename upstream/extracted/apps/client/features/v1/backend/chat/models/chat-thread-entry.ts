import type { FrontEndChatThreadModel, FrontendMessage } from "../new/models"

export async function handleCreateProjectChatThread(_projectId: string): Promise<string> { return "" }
export async function handleDeleteChatThread(_threadId: string): Promise<boolean> { return true }
export async function handleFindAllChatThreadsForCurrentUser(): Promise<FrontEndChatThreadModel[]> { return [] }
export async function handleUpdateChatTitle(_threadId: string, _messages: FrontendMessage[]): Promise<FrontEndChatThreadModel> { return {} as FrontEndChatThreadModel }
export async function handleUpsertChatThread(_thread: unknown): Promise<FrontEndChatThreadModel> { return {} as FrontEndChatThreadModel }
