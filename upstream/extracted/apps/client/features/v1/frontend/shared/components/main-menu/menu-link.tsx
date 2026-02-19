"use client"
import Link from "next/link"
import { usePathname } from "next/navigation"
import { cloneElement, type FC, isValidElement, useState } from "react"
import { cn } from "@/features/v1/frontend/shared/ui/lib"
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from "../../ui/tooltip"

interface MenuLinkProps {
	href: string
	ariaLabel: string
	children: React.ReactNode
	target?: string // Add target prop
	rel?: string // Add rel prop
	label?: string // Optional label for the link
}

export const MenuLink: FC<MenuLinkProps> = (props) => {
	const path = usePathname()
	const [isHovered, setIsHovered] = useState(false)
	const isActive = path.startsWith(props.href) && props.href !== "/"
	const enhancedChildIcon = isValidElement(props.children)
		? cloneElement(props.children as React.ReactElement<{ fill?: boolean }>, {
				fill: isActive || isHovered,
			})
		: props.children
	return (
		<>
			<TooltipProvider delayDuration={0}>
				<Tooltip>
					<TooltipTrigger asChild>
						<Link
							className={cn("w-full flex items-center justify-center h-full", {
								"text-main-menu-active": isActive,
								"text-main-menu-foreground": !isActive,
							})}
							href={props.href}
							aria-label={props.ariaLabel}
							target={props.target} // Pass target prop
							rel={props.rel} // Pass rel prop
							onMouseEnter={() => setIsHovered(true)}
							onMouseLeave={() => setIsHovered(false)}
							onClick={() => {
								// chat store stuff used to be here.
							}}
						>
							{enhancedChildIcon}
						</Link>
					</TooltipTrigger>
					<TooltipContent
						align="center"
						side="right"
						sideOffset={8}
						className="p-0 border-0 bg-transparent shadow-none"
					>
						<div className="bg-main-menu-tooltip-background ml-main-menu-tooltip text-main-menu-tooltip-foreground font-light  rounded-sm px-3 py-1.5 whitespace-nowrap relative ">
							{props.label}
						</div>
					</TooltipContent>
				</Tooltip>
			</TooltipProvider>

			{isActive && (
				<div
					className={`absolute h-12 w-main-menu-active-bar bg-white left-0 transition-transform duration-300 ease-in-out translate-x-[60px]`}
				/>
			)}
		</>
	)
}
