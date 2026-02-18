import { proxy, subscribe } from "valtio"
import {
	handleDeleteChatThread,
	handleFindAllChatThreadsForCurrentUser,
	handleUpdateChatTitle,
	handleUpsertChatThread,
} from "@/features/v1/backend/chat/models/chat-thread-entry"
import type {
	FrontEndChatThreadModel,
} from "@/features/v1/backend/chat/new/models"
import type { FrontendMessage } from "@/features/v1/backend/chat/new/models"
import { handleFindAllExtensionsForCurrentUser } from "@/features/v1/backend/extensions/models/extension-entry"
import type { FrontEndExtensionModel } from "@/features/v1/backend/extensions/models/models"
import type {
	FrontEndProjectModel,
	FrontEndRelationshipModel,
} from "@/features/v1/backend/projects/models/models"
import { showError } from "@/features/v1/frontend/shared/ui/global-message-store"
import { RedirectToPage } from "@/features/v1/lib/helpers/navigation-helpers"
import { useSnapshot } from "@/features/v1/lib/utils/valtio-hooks"
import { projectStore } from "../../stores/project-store"

class Menu {
	public isMenuOpen: boolean
	public chats: FrontEndChatThreadModel[] = []
	public projects: FrontEndProjectModel[] = []
	public relationships: FrontEndRelationshipModel[] = []
	public extensions: FrontEndExtensionModel[] = []
	public isLoading: boolean = false

	constructor() {
		this.isMenuOpen = true
		subscribe(projectStore, () => {
		})
	}

	toggleMenu() {
		this.isMenuOpen = !this.isMenuOpen
	}

	public setIsMenuOpen(isMenuOpen: boolean) {
		this.isMenuOpen = isMenuOpen
	}

	public setChats(chats: FrontEndChatThreadModel[]) {
		this.chats = chats
	}

	public addChat(chat: FrontEndChatThreadModel) {
		this.chats = [chat, ...this.chats]
	}

	public removeChat(chat: FrontEndChatThreadModel) {
		this.chats = this.chats.filter((c) => c.id !== chat.id)
	}

	public setExtensions(extensions: FrontEndExtensionModel[]) {
		this.extensions = extensions
	}

	public setLoading(loading: boolean) {
		this.isLoading = loading
	}
}

export const menuStore = proxy(new Menu())

// Hook to use the menu state
export const useMenuState = () => {
	return useSnapshot(menuStore)
}

export const initializeMenuData = async () => {
	try {
		menuStore.setLoading(true)
		const [chats, extensions] = await Promise.all([
			handleFindAllChatThreadsForCurrentUser(),
			handleFindAllExtensionsForCurrentUser(),
		])

		menuStore.setChats(chats)
		menuStore.setExtensions(extensions)
		menuStore.setLoading(false)
	} catch (error) {
		menuStore.setLoading(false)
		showError(`Error initializing menu data: ${error}`)
	}
}

export const updateChatThread = async (chatThread: FrontEndChatThreadModel) => {
	const originalChats = [...menuStore.chats]

	try {
		const updatedChats = menuStore.chats.map((chat) =>
			chat.id === chatThread.id ? { ...chat, ...chatThread } : chat,
		)
		menuStore.setChats(updatedChats)

		//TODO: Move to new chat-service.ts
		const updatedThread = await handleUpsertChatThread(chatThread)

		// Update with the response from the server
		const finalChats = menuStore.chats.map((chat) =>
			chat.id === chatThread.id ? { ...chat, ...updatedThread } : chat,
		)
		menuStore.setChats(finalChats)
	} catch (error) {
		showError(`Error updating chat thread: ${(error as Error).message}`)
		// Revert to original state on error
		menuStore.setChats(originalChats)
	}
}

export const deleteChatThread = async (chatThreadId: string) => {
	const originalChats = [...menuStore.chats]

	try {
		// Optimistically remove from the store
		const updatedChats = menuStore.chats.filter((chat) => chat.id !== chatThreadId)
		menuStore.setChats(updatedChats)

		const currentUrl = window.location.pathname
		if (currentUrl.includes(`/chat/${chatThreadId}`)) {
			RedirectToPage("chat")
		}

		await handleDeleteChatThread(chatThreadId)
	} catch (error) {
		showError(`Error deleting chat thread: ${(error as Error).message}`)
		// Revert to original state on error
		menuStore.setChats(originalChats)
	}
}

export async function generateNewChatThreadName(
	chatThreadId: string,
	messages: FrontendMessage[]
): Promise<FrontEndChatThreadModel | undefined> {
	try {
		const updatedChat = await handleUpdateChatTitle(chatThreadId, messages)

		const existingChatIndex = menuStore.chats.findIndex(chat => chat.id === chatThreadId)
 
		if (existingChatIndex !== -1) {
			const updatedChats = [...menuStore.chats]
			updatedChats[existingChatIndex] = updatedChat
			menuStore.setChats(updatedChats)
		} else {
			menuStore.setChats([updatedChat, ...menuStore.chats])
		}

		return updatedChat
	} catch (error) {
		showError(`error generating new chat thread name: ${(error as Error).message}`)
		return undefined
	}
}
