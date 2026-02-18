"use client"

import Link from "next/link"
import { usePathname } from "next/navigation"
import { cn } from "./lib"

interface BaseMenuItemProps {
	/** The navigation href for Link-based items. Mutually exclusive with onClick */
	href?: string
	/** Click handler for button-based items. Mutually exclusive with href */
	onClick?: () => void
	/** Icon element to display before the content */
	icon?: React.ReactNode
	/** The main content/text of the menu item */
	children: React.ReactNode
	/** Optional actions/context menu to display on the right */
	actions?: React.ReactNode
	/** Additional CSS classes */
	className?: string
	/** Custom active state check. If not provided, uses href-based matching */
	isActive?: boolean
	/** Enable drag functionality */
	draggable?: boolean
	/** Drag start handler */
	onDragStart?: (e: React.DragEvent) => void
	/** Data-testid attribute for testing */
	"data-testid"?: string
}

/**
 * Base menu item component that provides consistent styling for all menu items
 * across chat, personas, prompts, and groups menus.
 *
 * @example
 * // Link-based menu item
 * <BaseMenuItem href="/chat/123" icon={<ChatIcon />} actions={<ContextMenu />}>
 *   Chat Title
 * </BaseMenuItem>
 *
 * @example
 * // Button-based menu item
 * <BaseMenuItem onClick={handleClick} icon={<Icon />}>
 *   Click me
 * </BaseMenuItem>
 */
export const BaseMenuItem = ({
	"data-testid": dataTestId,
	href,
	onClick,
	icon,
	children,
	actions,
	className,
	isActive: customIsActive,
	draggable = false,
	onDragStart,
}: BaseMenuItemProps) => {
	const pathname = usePathname()
	const isActive = customIsActive ?? (href ? pathname.startsWith(href) && href !== "/" : false)

	const contentClasses = "flex-1 flex items-center gap-2 px-3 py-1.5 overflow-hidden min-w-0"

	const wrapperClasses = cn(
		"flex group transition text-menu-inactive-foreground rounded-md hover:bg-menu-item-hover/10",
		{
			"bg-menu-active text-menu-active-foreground hover:text-menu-active-foreground-hover hover:bg-menu-active-hover": isActive,
			"pr-1": actions, // Only add pr-1 if there are actions
		},
		className,
	)

	const content = (
		<>
			{icon}
			<span className="truncate flex-1">{children}</span>
		</>
	)

	if (onClick) {
		// Button-based menu item
		return (
			<div className={cn(wrapperClasses, "w-full")} data-testid={dataTestId}>
				<button
					type="button"
					onClick={onClick}
					className={cn(contentClasses, "text-left cursor-pointer")}
				>
					{content}
				</button>
				{actions}
			</div>
		)
	}

	if (href) {
		// Link-based menu item
		return (
			<li
				className={wrapperClasses}
				draggable={draggable}
				onDragStart={onDragStart}
				data-testid={dataTestId}
			>
				<Link href={href} className={contentClasses}>
					{content}
				</Link>
				{actions}
			</li>
		)
	}

	return null
}
