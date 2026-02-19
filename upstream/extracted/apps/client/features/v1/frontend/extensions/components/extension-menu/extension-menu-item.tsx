import Link from "next/link"
import { usePathname } from "next/navigation"
import type { FC } from "react"
import type { FrontEndExtensionModel } from "@/features/v1/backend/extensions/models/models"
import { cn } from "@/features/v1/frontend/shared/ui/lib"
import { MaterialSymbol } from "@/features/v1/frontend/shared/ui/material-symbol" // GAI-258: Using React 19 compatible MaterialSymbol wrapper instead of react-material-symbols
import { ExtensionCardContextMenu } from "../extension-card/extension-context-menu"

interface ExtensionMenuItemProps {
	extension: FrontEndExtensionModel
}

export const ExtensionMenuItem: FC<ExtensionMenuItemProps> = ({ extension }) => {
	const pathname = usePathname()
	const isActive = pathname.startsWith(`/extensions/${extension.id}`)

	return (
		<div
			data-testid={`extension-menu-item`}
			className={cn(
				"flex group hover:bg-menu-item-hover transition pr-3 text-menu-inactive-foreground rounded-md hover:text-menu-item-hover-foreground",
				{
					"bg-menu-active text-menu-active-foreground": isActive,
				},
			)}
		>
			<Link href={`/extensions/${extension.id}`} className="flex-1">
				<div className="flex-1 flex items-center gap-2 px-3 py-2 overflow-hidden">
					{extension.name}
				</div>
			</Link>
			<ExtensionCardContextMenu extension={extension} />
		</div>
	)
}
