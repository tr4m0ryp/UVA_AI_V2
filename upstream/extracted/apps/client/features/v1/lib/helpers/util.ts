import { customAlphabet } from "nanoid"
import type { FrontEndChatThreadModel } from "@/features/v1/backend/chat/new/models"
import type { ChatMessage } from "@/features/v1/backend/chat/new/models"
interface FileUIPart {
	type: string
	url?: string
	[key: string]: unknown
}

export const uniqueId = () => {
	const alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
	const nanoid = customAlphabet(alphabet, 36)
	return nanoid()
}

export const sortByTimestamp = (a: FrontEndChatThreadModel, b: FrontEndChatThreadModel) => {
	return new Date(b.lastMessageAt).getTime() - new Date(a.lastMessageAt).getTime()
}

export function getTextFromMessage(message: ChatMessage): string {
	return message.parts
		.filter((part) => part.type === "text")
		.map((part) => part.text)
		.join("")
}

export const convertBlobToDataUrl = async (file: FileUIPart): Promise<FileUIPart> => {
	if (!file.url?.startsWith('blob:')) {
		return file;
	}

	const response = await fetch(file.url);
	const blob = await response.blob();
	const dataUrl = await new Promise<string>((resolve) => {
		const reader = new FileReader();
		reader.onloadend = () => {
			resolve(reader.result as string);
		};
		reader.readAsDataURL(blob);
	});

	return {
		...file,
		url: dataUrl,
	} as FileUIPart;
};

export const isDatabaseError = (error: unknown): boolean => {
	if (error instanceof Error) {
		return (
			error.message.includes("Connection terminated") ||
			error.message.includes("connection timeout") ||
			error.message.includes("database error") ||
			error.message.includes("ECONNREFUSED") ||
			error.message.includes("ETIMEDOUT")
		)
	}
	return false
}