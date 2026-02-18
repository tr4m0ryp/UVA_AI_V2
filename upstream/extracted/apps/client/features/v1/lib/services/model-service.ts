export interface ModelOption {
	id: string
	name: string
	[key: string]: unknown
}

export async function fetchAvailableModelsAction(): Promise<ModelOption[]> { return [] }
