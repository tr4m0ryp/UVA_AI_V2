"use client"

import { Slot } from "@radix-ui/react-slot"
import { cva, type VariantProps } from "class-variance-authority"
import * as React from "react"
import { cn } from "./lib"

const buttonVariants = cva(
	"inline-flex items-center justify-center whitespace-nowrap rounded-md text-sm font-medium ring-offset-background transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 disabled:pointer-events-none disabled:opacity-50 cursor-pointer",
	{
		variants: {
			variant: {
				default: "bg-primary text-primary-foreground hover:bg-primary/90",
				destructive: "bg-destructive text-destructive-foreground hover:bg-destructive/90",
				outline: "border border-input bg-background hover:bg-accent hover:text-accent-foreground",
				secondary: "bg-secondary text-secondary-foreground hover:bg-secondary/80",
				ghost: "hover:bg-accent hover:text-accent-foreground",
				link: "text-primary underline-offset-4 hover:underline",
				mainMenu: "bg-main-menu text-main-menu-foreground hover:bg-main-menu-accent",
				submitChat: "bg-primary rounded-full text-primary-foreground hover:bg-primary/90",
				chatMessage: "bg-background hover:bg-accent text-foreground hover:text-foreground",
				newItem: "bg-transparent ",
				chatHeader: "bg-accent hover:bg-secondary text-foreground hover:text-foreground",
			},
			size: {
				default: "h-10 px-4 py-2",
				sm: "h-9 rounded-md px-3",
				lg: "h-11 rounded-md px-8",
				icon: "h-[32px] w-[32px]",
				chatMessage: "h-8 p-0",
			},
		},
		defaultVariants: {
			variant: "default",
			size: "default",
		},
	},
)

const ChatHeaderVariant = cn(
	buttonVariants({ variant: "chatHeader" }),
	"p-2 w-full h-12 flex items-center justify-center",
)

const ChatMessageVariant = cn(
	buttonVariants({ variant: "chatMessage", size: "chatMessage" }),
	"p-0 h-8 w-8 flex items-center justify-center",
)

const ButtonLinkVariant = cn(
	buttonVariants({ variant: "ghost" }),
	"p-0 w-full h-12 w-12 flex items-center justify-center ",
)

const mainMenuVariant = cn(
	buttonVariants({ variant: "mainMenu" }),
	"p-0 w-full h-12 w-12 flex items-center justify-center",
)

export interface ButtonProps
	extends React.ButtonHTMLAttributes<HTMLButtonElement>,
		VariantProps<typeof buttonVariants> {
	asChild?: boolean
}

const Button = React.forwardRef<HTMLButtonElement, ButtonProps>(
	({ className, variant, size, asChild = false, ...props }, ref) => {
		const Comp = asChild ? Slot : "button"
		return (
			<Comp className={cn(buttonVariants({ variant, size, className }))} ref={ref} {...props} />
		)
	},
)
Button.displayName = "Button"

export {
	Button,
	ButtonLinkVariant,
	buttonVariants,
	mainMenuVariant,
	ChatMessageVariant,
	ChatHeaderVariant,
}
