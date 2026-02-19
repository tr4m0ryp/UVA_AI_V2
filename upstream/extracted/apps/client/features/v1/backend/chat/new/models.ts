export interface FrontEndChatThreadModel {
	id: string
	name: string
	createdAt: Date
	lastMessageAt: string
	[key: string]: unknown
}

export interface FrontendMessage {
	id: string
	role: string
	content: string
	[key: string]: unknown
}

export interface ChatMessage {
	id: string
	role: string
	content: string
	parts: Array<{ type: string; text?: string; [key: string]: unknown }>
	[key: string]: unknown
}
