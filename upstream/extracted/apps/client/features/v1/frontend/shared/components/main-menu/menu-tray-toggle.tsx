"use client"
import { usePathname } from "next/navigation"
import { cn } from "@/features/v1/frontend/shared/ui/lib"
import { MaterialSymbol } from "@/features/v1/frontend/shared/ui/material-symbol" // GAI-258: Using React 19 compatible MaterialSymbol wrapper instead of react-material-symbols
import { MenuItem, menuIconProps } from "@/features/v1/frontend/shared/ui/menu"
import { menuStore, useMenuState } from "./menu-store"

export const MenuTrayToggle = () => {
	const { isMenuOpen } = useMenuState()
	return (
		<MenuItem onClick={() => menuStore.toggleMenu()} tooltip="Open and Collapse menu">
			<MaterialSymbol
				icon="left_panel_close"
				{...menuIconProps}
				className={cn("transition-all rotate-180 duration-700", isMenuOpen ? "rotate-0" : "")}
			/>
		</MenuItem>
	)
}
