// GAI-258: Converted from React.FC to function component syntax for React 19 compatibility
// GAI-258: Added TypeScript props typing to fix Biome unused interface warning
"use client"

import { BookmarkCheck, Pencil, Trash } from "lucide-react"
import { usePathname } from "next/navigation"
import { useTranslations } from "next-intl"
import { forwardRef, useState } from "react"
import type { FrontEndChatThreadModel } from "@/features/v1/backend/chat/new/models"
import type { FrontEndRelationshipModel } from "@/features/v1/backend/projects/models/models"
import { handleAddChatToProject } from "@/features/v1/backend/projects/models/project-entry"
import type {
	ProjectChild,
	ProjectNode,
} from "@/features/v1/frontend/projects/components/project-page"
import {
	deleteChatThread,
	updateChatThread,
	useMenuState,
} from "@/features/v1/frontend/shared/components/main-menu/menu-store"
import { BaseMenuItem } from "@/features/v1/frontend/shared/ui/base-menu-item"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import {
	Dialog,
	DialogContent,
	DialogHeader,
	DialogTitle,
	DialogTrigger,
} from "@/features/v1/frontend/shared/ui/dialog"
import {
	DropdownMenu,
	DropdownMenuContent,
	DropdownMenuItem,
	DropdownMenuSeparator,
} from "@/features/v1/frontend/shared/ui/dropdown-menu"
import { showError } from "@/features/v1/frontend/shared/ui/global-message-store"
import { Input } from "@/features/v1/frontend/shared/ui/input"
import { cn } from "@/features/v1/frontend/shared/ui/lib"
import { MaterialSymbol } from "@/features/v1/frontend/shared/ui/material-symbol" // GAI-258: Using React 19 compatible MaterialSymbol wrapper instead of react-material-symbols
import { MenuItemContextTrigger } from "@/features/v1/frontend/shared/ui/menu-item-context-trigger"
import { toast } from "@/features/v1/frontend/shared/ui/use-toast"
import { AddChatToProjectDialog } from "./add-chat-to-project-dialog"

interface ChatMenuItemProps {
	href: string
	chatThread: FrontEndChatThreadModel
	children?: React.ReactNode
	relationships: FrontEndRelationshipModel[]
	icon?: React.ReactNode
	dataTestId?: string
}

type DropdownAction = "bookmark" | "delete"

export const ChatMenuItem = (props: ChatMenuItemProps) => {
	const path = usePathname()
	const [isLoading, setIsLoading] = useState(false)

	const [isRenameDialogOpen, setIsRenameDialogOpen] = useState(false)
	const [isAddToProjectDialogOpen, setIsAddToProjectDialogOpen] = useState(false)

	const [renameValue, setRenameValue] = useState(props.chatThread.name)
	//const { projects, relationships, addChatToProject } = useChat();
	const { projects } = useMenuState()

	const t = useTranslations("Chats.chat-menu")

	const handleAction = async (action: DropdownAction) => {
		switch (action) {
			case "bookmark":
				setIsLoading(true)
				await updateChatThread({
					...props.chatThread,
					bookmarked: !props.chatThread.bookmarked,
				})
				setIsLoading(false)
				break
			case "delete":
				//TODO: proper delete dialog, perhaps tooltip sized instead of page level dialog.
				// we could implement a 'don't ask again' option to just skip the confirmation. Seems nice if you know what you are doing.
				if (window.confirm(t("delete-chat-confirm-notif"))) {
					setIsLoading(true)
					await deleteChatThread(props.chatThread.id)
					setIsLoading(false)
				}
				break
		}
	}

	const handleDragStart = (e: React.DragEvent) => {
		e.dataTransfer.effectAllowed = "move"
		e.dataTransfer.setData(
			"application/json",
			JSON.stringify({ chatId: props.chatThread.id, type: "chat" }),
		)
	}

	const handleRename = async () => {
		if (!renameValue.trim() || renameValue.length > 255) {
			showError(t("show-error-text"))
			return
		}
		setIsLoading(true)
		await updateChatThread({
			...props.chatThread,
			name: renameValue,
		})
		setIsLoading(false)
		setIsRenameDialogOpen(false)
	}

	const handleAddToProject = async (projectId: string) => {
		setIsLoading(true)
		const success = await handleAddChatToProject({
			parentId: projectId,
			childId: props.chatThread.id,
		})
		setIsLoading(false)
		setIsAddToProjectDialogOpen(false)
		if (success) {
			toast({
				title: t("chat-to-project-title-notif"),
				description: t("chat-to-project-desc-notif"),
			})
		}
	}

	// Convert to map bc NestedSearch component takes projectMap.
	const projectMap = new Map<string, ProjectNode>(
		projects.map(
			(project) =>
				[
					project.id,
					{
						...project,
						children: [] as ProjectChild[], // expliciete type assignment
					} as ProjectNode,
				] as const,
		),
	)

	// Deze block is een lazy fix ik heb weet niet zeker of dit de place to be is of ergens anders.
	props.relationships.forEach((rel) => {
		if (rel.childType === "directory") {
			const parentProject = projectMap.get(rel.parentId)
			const childProject = projectMap.get(rel.childId)
			if (parentProject && childProject) {
				parentProject.children.push(childProject)
			}
		}
	})

	const isActive = path.startsWith(props.href) && props.href !== "/"

	const actions = (
		<DropdownMenu modal={false}>
			<MenuItemContextTrigger isLoading={isLoading} ariaLabel="Chat Menu Item Dropdown Menu" />
			<DropdownMenuContent side="right" align="start">
				<DropdownMenuItemWithIcon onClick={() => handleAction("bookmark")}>
					<BookmarkCheck size={18} />
					<span>{props.chatThread.bookmarked ? t("remove-bookmark-btn") : t("bookmark-btn")}</span>
				</DropdownMenuItemWithIcon>

				<Dialog open={isRenameDialogOpen} onOpenChange={setIsRenameDialogOpen}>
					<DialogTrigger asChild>
						<DropdownMenuItemWithIcon
							onSelect={(event) => {
								event.preventDefault()
								setIsRenameDialogOpen(true)
							}}
						>
							<Pencil size={18} />
							<span>{t("rename-label")}</span>
						</DropdownMenuItemWithIcon>
					</DialogTrigger>
					<DialogContent>
						<DialogHeader>
							<DialogTitle>{t("rename-chat-title")}</DialogTitle>
						</DialogHeader>
						<div className="flex flex-col gap-y-4 pt-4">
							<Input
								placeholder={t("rename-chat-placeholder")}
								value={renameValue}
								onChange={(e) => setRenameValue(e.target.value)}
								onKeyDown={(e) => e.key === "Enter" && handleRename()}
								className={cn({
									"border-red-500": renameValue.length > 255,
								})}
							/>
							<div className="flex justify-between text-sm text-muted-foreground">
								<span
									className={cn({
										"text-red-500": renameValue.length > 255,
									})}
								>
									{renameValue.length > 255 && t("name-length-warning")}
								</span>
								<span
									className={cn({
										"text-red-500": renameValue.length > 255,
									})}
								>
									{renameValue.length}/255
								</span>
							</div>
							<div className="flex justify-end">
								<Button
									onClick={handleRename}
									disabled={!renameValue.trim() || renameValue.length > 255 || isLoading}
									variant={renameValue.length > 255 ? "secondary" : "default"}
									className={cn({
										"opacity-50 cursor-not-allowed":
											!renameValue.trim() || renameValue.length > 255,
									})}
								>
									{t("rename-chat-btn")}
								</Button>
							</div>
						</div>
					</DialogContent>
				</Dialog>

				<Dialog open={isAddToProjectDialogOpen} onOpenChange={setIsAddToProjectDialogOpen}>
					<DialogTrigger asChild>
						<DropdownMenuItemWithIcon
							onSelect={(event) => {
								event.preventDefault()
								setIsAddToProjectDialogOpen(true)
							}}
						>
							<MaterialSymbol icon="folder_open" weight={400} size={18} />
							<span>{t("add-to-project-btn")}</span>
						</DropdownMenuItemWithIcon>
					</DialogTrigger>
					<AddChatToProjectDialog
						projectMap={projectMap}
						onAddToProject={handleAddToProject}
						isOpen={isAddToProjectDialogOpen}
						onOpenChange={setIsAddToProjectDialogOpen}
					/>
				</Dialog>

				<DropdownMenuSeparator />
				<DropdownMenuItemWithIcon onClick={() => handleAction("delete")}>
					<Trash size={18} />
					<span>{t("delete-btn")}</span>
				</DropdownMenuItemWithIcon>
			</DropdownMenuContent>
		</DropdownMenu>
	)

	return (
		<BaseMenuItem
			data-testid={props.dataTestId || "chat-menu-item"}
			href={props.href}
			icon={props.icon}
			isActive={isActive}
			actions={actions}
			draggable={true}
			onDragStart={handleDragStart}
		>
			{props.chatThread.name}
		</BaseMenuItem>
	)
}

//TODO: make this regular component somewhere else as it's used for multiple pages accross codebase
interface DropdownMenuItemWithIconProps
	extends React.ComponentPropsWithoutRef<typeof DropdownMenuItem> {
	children?: React.ReactNode
}

// Update DropdownMenuItemWithIcon to accept `asChild`
export const DropdownMenuItemWithIcon = forwardRef<HTMLDivElement, DropdownMenuItemWithIconProps>(
	({ children, className = "", ...props }, ref) => {
		return (
			<DropdownMenuItem {...props} ref={ref} className={`flex gap-2 ${className}`}>
				{children}
			</DropdownMenuItem>
		)
	},
)
