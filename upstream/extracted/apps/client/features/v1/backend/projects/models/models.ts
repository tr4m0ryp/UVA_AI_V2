export interface FrontEndProjectModel {
	id: string
	name: string
	parentId: string | null
	createdAt: Date
	color: string | null
	customInstructions: string | null
	[key: string]: unknown
}

export interface FrontEndRelationshipModel {
	id: string
	parentId: string
	childId: string
	childType: string
	[key: string]: unknown
}
