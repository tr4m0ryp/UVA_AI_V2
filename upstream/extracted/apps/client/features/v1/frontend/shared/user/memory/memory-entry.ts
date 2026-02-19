import type { MemoryModel } from "./models"

export async function handleFindAllMemoriesForCurrentUser(): Promise<MemoryModel[]> { return [] }
export async function handleCreateMemory(_content: string): Promise<MemoryModel> { return {} as MemoryModel }
export async function handleUpdateMemory(_id: string, _content: string): Promise<MemoryModel> { return {} as MemoryModel }
export async function handleDeleteMemory(_id: string): Promise<boolean> { return true }
export async function handleDeleteAllMemories(): Promise<boolean> { return true }
