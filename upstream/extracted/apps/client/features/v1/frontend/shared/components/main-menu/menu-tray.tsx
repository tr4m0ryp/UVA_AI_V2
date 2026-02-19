"use client"

import React from "react"
import { cn } from "@/features/v1/frontend/shared/ui/lib"
import { useMenuState } from "./menu-store"

export const MenuTray = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
	({ className, ...props }, ref) => {
		const { isMenuOpen } = useMenuState()
		return (
			<div
				className={cn(
					"transition-[width] duration-700 overflow-hidden",
					isMenuOpen ? "w-96" : "w-0",
				)}
			>
				<div
					ref={ref}
					className={cn("flex flex-col border-r w-96 bg-menu h-full", className)}
					{...props}
				>
					{props.children}
				</div>
			</div>
		)
	},
)
MenuTray.displayName = "MenuTray"
