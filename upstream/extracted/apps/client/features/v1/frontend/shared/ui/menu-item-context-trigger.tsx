"use client"

import { MoreVertical } from "lucide-react"
import { DropdownMenuTrigger } from "./dropdown-menu"
import { LoadingIndicator } from "./loading"

interface MenuItemContextTriggerProps {
	isLoading?: boolean
	disabled?: boolean
	ariaLabel?: string
}

export const MenuItemContextTrigger = ({
	isLoading = false,
	disabled = false,
	ariaLabel = "Menu options",
}: MenuItemContextTriggerProps) => {
	return (
		<DropdownMenuTrigger
			disabled={disabled || isLoading}
			className="hover:!bg-primary/10 rounded-md p-1 transition self-center relative z-10"
		>
			{isLoading ? (
				<LoadingIndicator isLoading={isLoading} />
			) : (
				<MoreVertical
					className="opacity-0 group-hover:opacity-100 transition-all duration-200"
					size={18}
					aria-label={ariaLabel}
				/>
			)}
		</DropdownMenuTrigger>
	)
}
