"use client"

import * as DialogPrimitive from "@radix-ui/react-dialog"
import { X } from "lucide-react"
import * as React from "react"

import { cn } from "@/features/v1/frontend/shared/ui/lib"

const Dialog = DialogPrimitive.Root

const DialogTrigger = DialogPrimitive.Trigger

const DialogPortal = DialogPrimitive.Portal

const DialogClose = DialogPrimitive.Close

interface DialogOverlayProps
	extends React.ComponentPropsWithoutRef<typeof DialogPrimitive.Overlay> {
	variant?: "default" | "transparent"
}

const DialogOverlay = React.forwardRef<
	React.ComponentRef<typeof DialogPrimitive.Overlay>,
	DialogOverlayProps
>(({ className, variant = "default", ...props }, ref) => (
	<DialogPrimitive.Overlay
		ref={ref}
		className={cn(
			"fixed inset-0 z-50",
			variant === "default" ? "bg-black/80" : "bg-transparent",
			className,
		)}
		{...props}
	/>
))
DialogOverlay.displayName = DialogPrimitive.Overlay.displayName

interface DialogContentProps
	extends React.ComponentPropsWithoutRef<typeof DialogPrimitive.Content> {
	variant?: "default" | "transparent"
	position?: "center" | "top" //top pins the dialog - useful when the height will change (like a search dialog)
}

const DialogContent = React.forwardRef<
	React.ComponentRef<typeof DialogPrimitive.Content>,
	DialogContentProps
>(({ className, children, variant = "default", position = "center", ...props }, ref) => (
	<DialogPortal>
		<DialogOverlay variant={variant} />
		<DialogPrimitive.Content
			ref={ref}
			className={cn(
				"fixed left-[50%] z-50 grid w-[calc(100%-2rem)] max-w-lg translate-x-[-50%] gap-4 border bg-background p-6 shadow-lg rounded-lg",
				"focus:outline-none focus-visible:ring-0",
				position === "center" && "top-[50%] translate-y-[-50%]",
				position === "top" && "top-[20%]",
				className,
			)}
			{...props}
		>
			{children}
			<DialogPrimitive.Close className="absolute right-4 top-4 rounded-sm opacity-70 ring-offset-background transition-opacity hover:opacity-100 focus:outline-none focus:ring-2 focus:ring-ring focus:ring-offset-2 disabled:pointer-events-none data-[state=open]:bg-accent data-[state=open]:text-muted-foreground cursor-pointer">
				<X className="h-4 w-4" />
				<span className="sr-only">Close</span>
			</DialogPrimitive.Close>
		</DialogPrimitive.Content>
	</DialogPortal>
))
DialogContent.displayName = DialogPrimitive.Content.displayName

const DialogHeader = ({ className, ...props }: React.HTMLAttributes<HTMLDivElement>) => (
	<div className={cn("flex flex-col gap-y-1.5 text-center sm:text-left", className)} {...props} />
)
DialogHeader.displayName = "DialogHeader"

const DialogFooter = ({ className, ...props }: React.HTMLAttributes<HTMLDivElement>) => (
	<div
		className={cn("flex flex-col-reverse sm:flex-row sm:justify-end sm:gap-x-2", className)}
		{...props}
	/>
)
DialogFooter.displayName = "DialogFooter"

const DialogTitle = React.forwardRef<
	React.ComponentRef<typeof DialogPrimitive.Title>,
	React.ComponentPropsWithoutRef<typeof DialogPrimitive.Title>
>(({ className, ...props }, ref) => (
	<DialogPrimitive.Title
		ref={ref}
		className={cn("text-lg font-semibold leading-none tracking-tight", className)}
		{...props}
	/>
))
DialogTitle.displayName = DialogPrimitive.Title.displayName

const DialogDescription = React.forwardRef<
	React.ComponentRef<typeof DialogPrimitive.Description>,
	React.ComponentPropsWithoutRef<typeof DialogPrimitive.Description>
>(({ className, ...props }, ref) => (
	<DialogPrimitive.Description
		ref={ref}
		className={cn("text-sm text-muted-foreground", className)}
		{...props}
	/>
))
DialogDescription.displayName = DialogPrimitive.Description.displayName

export {
	Dialog,
	DialogClose,
	DialogContent,
	DialogDescription,
	DialogFooter,
	DialogHeader,
	DialogOverlay,
	DialogPortal,
	DialogTitle,
	DialogTrigger,
}
