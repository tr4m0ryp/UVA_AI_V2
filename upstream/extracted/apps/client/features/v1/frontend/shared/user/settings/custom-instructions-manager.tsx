"use client"

import { AlertCircle } from "lucide-react"
import type React from "react"
import { useEffect, useState } from "react"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import {
	Dialog,
	DialogContent,
	DialogHeader,
	DialogTitle,
} from "@/features/v1/frontend/shared/ui/dialog"
import { LoadingIndicator } from "@/features/v1/frontend/shared/ui/loading"
import { Textarea } from "@/features/v1/frontend/shared/ui/textarea"
import { toast } from "@/features/v1/frontend/shared/ui/use-toast"
import { CheckSystemPromptSafety } from "../memory/memory-service"
import { UserSettingKey } from "../models"
import { userStore, useUser } from "../user-store"
import { useSettings } from "./settings-provider"

export function InstructionsManager() {
	const { isInstructionManagerOpen } = useUser()
	const { setSetting, settings, isLoading: areSettingsLoading } = useSettings()
	const [instructions, setInstructions] = useState<string>("")
	const [isLoading, setIsLoading] = useState<boolean>(false)
	const characterLimit = 1500
	const characterCount = instructions.length
	const isOverLimit = characterCount > characterLimit

	const handleOpenChange = (open: boolean) => {
		if (!open) {
			userStore.toggleInstructionsManager(false)
		}
	}

	useEffect(() => {
		if (isInstructionManagerOpen && !areSettingsLoading && settings) {
			const savedInstructions = settings.get(UserSettingKey.CUSTOM_SYSTEM_PROMPT)
			setInstructions(savedInstructions || "")
		}
	}, [isInstructionManagerOpen, settings, areSettingsLoading])

	const handleInstructionsChange = (e: React.ChangeEvent<HTMLTextAreaElement>) => {
		setInstructions(e.target.value)
	}

	const handleSave = async () => {
		if (isOverLimit) return

		setIsLoading(true)
		const valid = await CheckSystemPromptSafety(instructions)

		if (!valid) {
			toast({
				title: "Instructions not safe",
				description:
					"Please make sure the instructions are safe and do not contain harmful content.",
			})
		} else {
			setSetting(UserSettingKey.CUSTOM_SYSTEM_PROMPT, instructions) //TODO: Custom backend functie met safety verification.
			handleOpenChange(false)
		}
		setIsLoading(false)
	}

	const handleClear = () => {
		setInstructions("")
	}

	return (
		<Dialog open={isInstructionManagerOpen} onOpenChange={handleOpenChange}>
			<DialogContent
				variant={"transparent"}
				className="sm:max-w-[600px] bg-background border-border"
			>
				<DialogHeader>
					<DialogTitle className="text-xl font-normal text-foreground">
						Custom Instructions
					</DialogTitle>
				</DialogHeader>

				<div className="flex flex-col gap-4">
					<div className="relative">
						<Textarea
							value={instructions}
							onChange={handleInstructionsChange}
							placeholder={
								areSettingsLoading
									? "Loading instructions..."
									: "Add your custom instructions here..."
							}
							disabled={areSettingsLoading || isLoading}
							className={`min-h-[200px] resize-none p-4 text-sm ${
								isOverLimit ? "border-red-500 focus-visible:ring-red-500" : ""
							}`}
						/>

						<div
							className={`flex justify-between items-center mt-2 text-sm ${
								isOverLimit
									? "text-red-500"
									: characterCount > characterLimit * 0.9
										? "text-amber-500"
										: "text-muted-foreground"
							}`}
						>
							<div className="flex items-center gap-1">
								{isOverLimit && (
									<>
										<AlertCircle className="h-4 w-4" />
										<span>Character limit exceeded</span>
									</>
								)}
							</div>
							<div>
								{characterCount}/{characterLimit} characters
							</div>
						</div>
					</div>

					<div className="flex justify-between mt-4">
						<Button
							variant="outline"
							size="sm"
							onClick={handleClear}
							disabled={areSettingsLoading || isLoading}
						>
							Clear
						</Button>
						{isLoading ? (
							<Button variant="default" size="sm">
								<LoadingIndicator isLoading />
							</Button>
						) : (
							<Button
								variant="default"
								size="sm"
								onClick={handleSave}
								disabled={isOverLimit || areSettingsLoading}
							>
								Save Instructions
							</Button>
						)}
					</div>
				</div>
			</DialogContent>
		</Dialog>
	)
}
