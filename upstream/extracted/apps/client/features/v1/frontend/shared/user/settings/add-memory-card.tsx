"use client"

import { Check, Plus } from "lucide-react"
import { useEffect, useRef, useState } from "react"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import { Input } from "@/features/v1/frontend/shared/ui/input"
import { userStore } from "../user-store"

export function AddMemoryCard() {
	const [isEditing, setIsEditing] = useState(false)
	const [content, setContent] = useState("")
	const [isSubmitting, setIsSubmitting] = useState(false)
	const [isFocused, setIsFocused] = useState(false)
	const inputRef = useRef<HTMLInputElement>(null)

	useEffect(() => {
		if (isEditing && inputRef.current) {
			inputRef.current.focus()
		}
	}, [isEditing])

	const handleSave = async () => {
		if (!content.trim()) return

		setIsSubmitting(true)
		try {
			await userStore.CreateMemory(content)
			setContent("")
			setIsEditing(false)
		} catch (error) {
			console.error("Failed to save memory:", error)
		} finally {
			setIsSubmitting(false)
		}
	}

	if (!isEditing) {
		return (
			<Button
				data-testid="add-memory-card-button"
				variant="ghost"
				className="flex items-center justify-center w-full h-[68px] rounded-lg border border-dashed border-offset hover:border-foreground hover:bg-muted group"
				onClick={() => setIsEditing(true)}
			>
				<Plus className="h-6 w-6 text-muted-foreground group-hover:text-foreground" />
			</Button>
		)
	}

	return (
		<div className={`flex items-start justify-between gap-2 rounded-lg p-4 text-sm border border-offset bg-muted dark:bg-gray-900 ${isFocused ? "[box-shadow:inset_0_0_0_2px_hsl(var(--primary))]" : ""}`}>
			<Input
				ref={inputRef}
				data-testid="add-memory-card-input"
				value={content}
				onChange={(e) => setContent(e.target.value)}
				onFocus={() => setIsFocused(true)}
				onBlur={() => setIsFocused(false)}
				placeholder="Type your memory here..."
				className="flex-1 bg-transparent border-0 p-2 shadow-none  focus-visible:ring-0 focus-visible:ring-offset-0 focus-visible:outline-none"
				disabled={isSubmitting}
			/>
			<Button
				data-testid="add-memory-card-save-button"
				variant="ghost"
				size="icon"
				onClick={handleSave}
				disabled={isSubmitting || !content.trim()}
				className="text-muted-foreground hover:text-green-700"
			>
				<Check className="h-5 w-5" />
				<span className="sr-only">Save memory</span>
			</Button>
		</div>
	)
}
