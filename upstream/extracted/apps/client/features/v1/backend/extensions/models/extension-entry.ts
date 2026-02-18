import type { FrontEndExtensionModel } from "./models"

export async function handleCreateExtension(_input: unknown): Promise<FrontEndExtensionModel> { return {} as FrontEndExtensionModel }
export async function handleDeleteExtension(_id: string): Promise<boolean> { return true }
export async function handleFindAllExtensionsForCurrentUser(): Promise<FrontEndExtensionModel[]> { return [] }
export async function handleUpdateExtension(_input: unknown): Promise<FrontEndExtensionModel> { return {} as FrontEndExtensionModel }
