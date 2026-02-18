"use client"

import * as ScrollAreaPrimitive from "@radix-ui/react-scroll-area"
import * as React from "react"

import { cn } from "@/features/v1/frontend/shared/ui/lib"

const ScrollArea = React.forwardRef<
	React.ComponentRef<typeof ScrollAreaPrimitive.Root>,
	React.ComponentPropsWithoutRef<typeof ScrollAreaPrimitive.Root>
>(({ className, children, ...props }, ref) => {
	const scrollRef = React.useRef<HTMLDivElement>(null)

	React.useEffect(() => {
		const handleScroll = () => {
			if (scrollRef.current) {
				scrollRef.current.scrollTop = scrollRef.current.scrollHeight ?? 0
			}
		}

		const scrollElement = scrollRef.current

		if (scrollElement) {
			scrollElement.addEventListener("scroll", handleScroll)
			scrollElement.addEventListener("touchstart", handleScroll, {
				passive: false,
			})
		}

		return () => {
			if (scrollElement) {
				scrollElement.removeEventListener("scroll", handleScroll)
			}
		}
	}, [])
	return (
		<ScrollAreaPrimitive.Root className={cn("relative overflow-hidden", className)} {...props}>
			<ScrollAreaPrimitive.Viewport
				ref={ref}
				className="h-full w-full rounded-[inherit] [&>div]:block!"
			>
				{children}
			</ScrollAreaPrimitive.Viewport>
			<ScrollBar />
			<ScrollAreaPrimitive.Corner />
		</ScrollAreaPrimitive.Root>
	)
})
ScrollArea.displayName = ScrollAreaPrimitive.Root.displayName

const ScrollBar = React.forwardRef<
	React.ComponentRef<typeof ScrollAreaPrimitive.ScrollAreaScrollbar>,
	React.ComponentPropsWithoutRef<typeof ScrollAreaPrimitive.ScrollAreaScrollbar>
>(({ className, orientation = "vertical", ...props }, ref) => (
	<ScrollAreaPrimitive.Scrollbar
		ref={ref}
		orientation={orientation}
		className={cn(
			"flex touch-none select-none transition-colors",
			orientation === "vertical" && "h-full w-1.5 hover:w-2.5 border-l border-l-transparent p-px",
			orientation === "horizontal" && "h-2.5 flex-col border-t border-t-transparent p-px",
			className,
		)}
		{...props}
	>
		<ScrollAreaPrimitive.Thumb className="relative flex-1 rounded-full bg-border" />
	</ScrollAreaPrimitive.Scrollbar>
))
ScrollBar.displayName = ScrollAreaPrimitive.ScrollAreaScrollbar.displayName

export { ScrollArea, ScrollBar }
