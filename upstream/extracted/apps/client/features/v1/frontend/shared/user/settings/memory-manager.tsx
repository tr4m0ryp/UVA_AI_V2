"use client"

import { Check, Pencil, Trash2, X } from "lucide-react"
import { useEffect, useRef, useState } from "react"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import {
	Dialog,
	DialogContent,
	DialogHeader,
	DialogTitle,
} from "@/features/v1/frontend/shared/ui/dialog"
import { Input } from "@/features/v1/frontend/shared/ui/input"
import { deleteMemory, updateMemory, userStore, useUser } from "../user-store"
import { AddMemoryCard } from "./add-memory-card"

interface EditingMemory {
	id: string
	content: string
}

export function MemoryManager() {
	const { memory, isMemoryManagerOpen, isSettingsOpen } = useUser()
	const [editingMemory, setEditingMemory] = useState<EditingMemory | null>(null)
	const [isInputFocused, setIsInputFocused] = useState(false)
	const inputRef = useRef<HTMLInputElement>(null)

	useEffect(() => {
		if (editingMemory && inputRef.current) {
			inputRef.current.focus()
		}
	}, [editingMemory])

	const handleOpenChange = (open: boolean) => {
		if (!open) {
			userStore.toggleMemoryManager(false)
		}
	}

	const handleEdit = (memory: { id: string; content: string }) => {
		setEditingMemory({ id: memory.id, content: memory.content })
	}

	const handleSaveEdit = async () => {
		if (!editingMemory) return
		await updateMemory(editingMemory.id, editingMemory.content)
		setEditingMemory(null)
		setIsInputFocused(false)
	}

	const handleCancelEdit = () => {
		setEditingMemory(null)
		setIsInputFocused(false)
	}

	return (
		<Dialog open={isMemoryManagerOpen} onOpenChange={handleOpenChange}>
			<DialogContent
				variant={isSettingsOpen ? "transparent" : "default"}
				className="sm:max-w-[600px] bg-background border-border"
			>
				<DialogHeader>
					<DialogTitle className="text-xl font-normal text-foreground">Memory</DialogTitle>
				</DialogHeader>
				<div className="flex flex-col gap-2 max-h-[60vh] overflow-y-auto pr-2">
					<AddMemoryCard />

					{memory.map((memory) => (
						<div
							data-testid="memory-manager-item"
							key={memory.id}
							className={`flex items-start justify-between group rounded-lg p-4 text-sm border border-offset bg-muted dark:bg-gray-900 text-foreground ${editingMemory?.id === memory.id && isInputFocused ? "[box-shadow:inset_0_0_0_2px_hsl(var(--primary))]" : ""}`}
						>
							{editingMemory?.id === memory.id ? (
								<>
									<Input
										ref={inputRef}
										data-testid="memory-manager-edit-input"
										value={editingMemory.content}
										onChange={(e) =>
											setEditingMemory({ ...editingMemory, content: e.target.value })
										}
										onFocus={() => setIsInputFocused(true)}
										onBlur={() => setIsInputFocused(false)}
										className="flex-1 bg-transparent border-0 p-2 mr-1 shadow-none focus-visible:ring-0 focus-visible:ring-offset-0 focus-visible:outline-none"
									/>
									<div className="flex gap-1">
										<Button
											data-testid="memory-manager-save-edit-button"
											variant="ghost"
											size="icon"
											onClick={handleSaveEdit}
											disabled={!editingMemory.content.trim()}
											className="text-muted-foreground hover:text-green-700"
										>
											<Check className="h-5 w-5" />
											<span className="sr-only">Save edit</span>
										</Button>
										<Button
											data-testid="memory-manager-cancel-edit-button"
											variant="ghost"
											size="icon"
											onClick={handleCancelEdit}
											className="text-muted-foreground hover:text-foreground"
										>
											<X className="h-5 w-5" />
											<span className="sr-only">Cancel edit</span>
										</Button>
									</div>
								</>
							) : (
								<>
									<p className="leading-relaxed p-2">{memory.content}</p>
									<div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
										<Button
											data-testid="memory-manager-edit-button"
											variant="ghost"
											size="icon"
											className="text-muted-foreground hover:text-foreground"
											onClick={() => handleEdit(memory)}
										>
											<Pencil className="h-4 w-4" />
											<span className="sr-only">Edit memory</span>
										</Button>
										<Button
											data-testid="memory-manager-delete-button"
											variant="ghost"
											size="icon"
											className="text-muted-foreground hover:text-foreground"
											onClick={() => deleteMemory(memory.id)}
										>
											<Trash2 className="h-4 w-4" />
											<span className="sr-only">Delete memory</span>
										</Button>
									</div>
								</>
							)}
						</div>
					))}
				</div>
				<div className="flex justify-end mt-4">
					<Button
						data-testid="memory-manager-clear-button"
					 	variant="destructive" size="sm" onClick={() => userStore.clearMemories()}>
						Clear memories
					</Button>
				</div>
			</DialogContent>
		</Dialog>
	)
}
