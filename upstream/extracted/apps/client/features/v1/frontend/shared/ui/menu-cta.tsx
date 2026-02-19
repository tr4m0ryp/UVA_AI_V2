"use client"

import { PlusIcon } from "lucide-react"
import Link from "next/link"
import { Button } from "./button"

interface MenuCTAProps {
	label: string
	href?: string
	onAction?: () => void
	"data-testid"?: string
}

export const MenuCTA = ({ label, href, onAction, "data-testid": dataTestId }: MenuCTAProps) => {
	const iconElement = <PlusIcon size={18} strokeWidth="2px" className="text-menu-cta-foreground" />

	const buttonContent = (
		<>
			{iconElement}
			{label}
		</>
	)

	const buttonClasses =
		"w-full flex group hover:bg-menu-cta-background-hover/10 pr-3 pl-2.5 text-md justify-start gap-2 font-medium items-center hover:cursor-pointer text-menu-cta-foreground hover:text-menu-cta-foreground-hover rounded-md -mt-2.5"

	if (href) {
		return (
			<Button className={buttonClasses} variant="ghost" asChild>
				<Link href={href} data-testid={dataTestId}>
					{buttonContent}
				</Link>
			</Button>
		)
	}

	if (onAction) {
		return (
			<form action={onAction} className="w-full">
				<Button type="submit" className={buttonClasses} variant="ghost" data-testid={dataTestId}>
					{buttonContent}
				</Button>
			</form>
		)
	}

	return null
}
