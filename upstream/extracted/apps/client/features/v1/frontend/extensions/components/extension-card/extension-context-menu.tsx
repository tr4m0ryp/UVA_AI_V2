"use client"

import { MoreVertical, Pencil, Trash } from "lucide-react"
import { useTranslations } from "next-intl"
import { type FC, useState } from "react"
import type { FrontEndExtensionModel } from "@/features/v1/backend/extensions/models/models"
import { DropdownMenuItemWithIcon } from "@/features/v1/frontend/chat/components/menu/chat-menu-item"
import {
	extensionStore,
	removeExtension,
} from "@/features/v1/frontend/shared/stores/extension-store"
import {
	DropdownMenu,
	DropdownMenuContent,
	DropdownMenuTrigger,
} from "@/features/v1/frontend/shared/ui/dropdown-menu"
import { LoadingIndicator } from "@/features/v1/frontend/shared/ui/loading"

interface Props {
	extension: FrontEndExtensionModel
}

type DropdownAction = "edit" | "delete"

export const ExtensionCardContextMenu: FC<Props> = (props) => {
	const { isLoading, handleAction } = useDropdownAction({
		extension: props.extension,
	})

	const t = useTranslations("Extensions.extension-card")

	return (
		<DropdownMenu modal={false}>
			<DropdownMenuTrigger>
				{isLoading ? (
					<LoadingIndicator isLoading={isLoading} />
				) : (
					<MoreVertical
						className="opacity-0 group-hover:opacity-100 transition-all duration-200"
						size={18}
						aria-label="Extension Menu Item Dropdown Menu"
					/>
				)}
			</DropdownMenuTrigger>
			<DropdownMenuContent>
				<DropdownMenuItemWithIcon onClick={() => extensionStore.editExtension(props.extension)}>
					<Pencil size={18} />
					<span>{t("edit-btn")}</span>
				</DropdownMenuItemWithIcon>
				<DropdownMenuItemWithIcon onClick={async () => await handleAction("delete")}>
					<Trash size={18} />
					<span>{t("delete-btn")}</span>
				</DropdownMenuItemWithIcon>
			</DropdownMenuContent>
		</DropdownMenu>
	)
}

const useDropdownAction = (props: { extension: FrontEndExtensionModel }) => {
	const [isLoading, setIsLoading] = useState(false)

	const handleAction = async (action: DropdownAction) => {
		setIsLoading(true)
		try {
			if (action === "delete") {
				await removeExtension(props.extension.id)
			}
		} catch (error) {
			console.error(error)
		}
		setIsLoading(false)
	}

	return {
		isLoading,
		handleAction,
	}
}
