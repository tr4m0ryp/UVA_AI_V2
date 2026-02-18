"use client"

import * as React from "react"
import { cn } from "@/features/v1/frontend/shared/ui/lib"
import type { MaterialSymbolProps } from "@/features/v1/frontend/shared/ui/material-symbol"
import { Button, type ButtonProps, mainMenuVariant } from "./button"
import { TooltipProvider } from "./tooltip"

const Menu = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
	({ className, ...props }, ref) => (
		<div ref={ref} className={cn("flex h-full", className)} {...props}>
			<TooltipProvider>{props.children}</TooltipProvider>
		</div>
	),
)
Menu.displayName = "Menu"

const MenuBar = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
	({ className, ...props }, ref) => (
		<div
			ref={ref}
			className={cn(
				"bg-main-menu z-20 py-3 w-[64px] flex flex-col justify-between  h-full items-center border-r",
				className,
			)}
			{...props}
		>
			<TooltipProvider>{props.children}</TooltipProvider>
		</div>
	),
)
MenuBar.displayName = "MenuBar"

type AnchorProps = ButtonProps & {
	tooltip?: string
}

const MenuItemContainer = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
	({ className, ...props }, ref) => (
		<div ref={ref} className={cn(" flex flex-col w-full", className)} {...props}>
			{props.children}
		</div>
	),
)
MenuItemContainer.displayName = "MenuItemContainer"

const MenuItem = React.forwardRef<HTMLButtonElement, AnchorProps>(
	({ className, variant = "mainMenu", size, asChild = false, ...props }, ref) => {
		return (
			<div className="w-full flex justify-center bg-main-menu">
				<Button
					// title={props.tooltip}
					variant={variant}
					asChild={asChild}
					{...props}
					className={cn(mainMenuVariant)}
					ref={ref}
				/>
			</div>
		)
	},
)
MenuItem.displayName = "MenuItem"

const MenuLabel = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
	({ className, content, ...props }, ref) => (
		<div
			ref={ref}
			className={cn("h-12 flex items-center text-md font-light text-white", className)}
			{...props}
		>
			{content}
		</div>
	),
)
MenuLabel.displayName = "MenuLabel"

const menuIconProps: Omit<MaterialSymbolProps, "icon"> = {
	size: 32,
	weight: 300,
}

export { Menu, MenuBar, MenuItem, MenuLabel, MenuItemContainer, menuIconProps }
