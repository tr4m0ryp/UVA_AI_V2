import { proxy } from "valtio"
import { toast } from "@/features/v1/frontend/shared/ui/use-toast"
import {
	handleCreateMemory,
	handleDeleteAllMemories,
	handleDeleteMemory,
	handleFindAllMemoriesForCurrentUser,
	handleUpdateMemory,
} from "@/features/v1/frontend/shared/user/memory/memory-entry"
import type { MemoryModel } from "@/features/v1/frontend/shared/user/memory/models"
import { RevalidateCache } from "@/features/v1/lib/helpers/navigation-helpers"
import { useSnapshot } from "@/features/v1/lib/utils/valtio-hooks"
import type { UserSettingKey } from "./models"

class UserState {
	public memory: ReadonlyArray<MemoryModel> | Array<MemoryModel> = []
	public isMemoryToggled: boolean = true
	public isSettingsOpen: boolean = false
	public isMemoryManagerOpen: boolean = false
	public isInstructionManagerOpen: boolean = false
	public settings: Map<UserSettingKey, string> = new Map()

	public async setMemory(memory: MemoryModel[]) {
		userStore.memory = memory
	}

	public async updateSettings(settings: Map<UserSettingKey, string>) {
		this.settings = new Map(settings)
	}

	public async CreateMemory(content: string) {
		if (typeof content !== "string") {
			toast({
				title: "Invalid input",
				description: "Please only write regular text.",
			})
			return
		}
		const response = await handleCreateMemory(content)

		if (response) {
			RevalidateCache({ type: "layout" })
			this.memory = [...this.memory, response]
		}
	}

	public async clearMemories() {
		const confirmed = window.confirm("Are you sure you want to delete all memories?")
		if (!confirmed) return
		const response = await handleDeleteAllMemories()

		if (response) {
			RevalidateCache({ type: "layout" })
			userStore.memory = []
		}
	}

	public toggleSettings = (isOpen: boolean) => {
		userStore.isSettingsOpen = isOpen
	}

	public toggleMemoryManager = (isOpen: boolean) => {
		userStore.isMemoryManagerOpen = isOpen
	}

	public toggleInstructionsManager = (isOpen: boolean) => {
		userStore.isInstructionManagerOpen = isOpen
	}
}

export const userStore = proxy(new UserState())

export const useUser = () => {
	return useSnapshot(userStore)
}

export const deleteMemory = async (id: string) => {
	const confirmed = window.confirm("Are you sure you want to delete this memory?") //TODO: Window.confirm vervangen met standaard dialog component na merge etc.
	if (!confirmed) return

	const response = await handleDeleteMemory(id)
	if (response) {
		userStore.memory = userStore.memory.filter((mem) => mem.id !== id)
		RevalidateCache({ type: "layout" })
	}
}

export const updateMemory = async (id: string, content: string) => {
	const response = await handleUpdateMemory(id, content)
	if (response) {
		RevalidateCache({
			page: "settings",
			type: "layout",
		})
	}
}

export const initializeMemory = async () => {
	const memory = await handleFindAllMemoriesForCurrentUser()
	userStore.setMemory(memory)
}
