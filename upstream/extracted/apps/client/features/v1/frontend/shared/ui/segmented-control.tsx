"use client"

import * as ToggleGroup from "@radix-ui/react-toggle-group"
import type React from "react"
import type { ReactNode } from "react"

type ChildProp = { children?: ReactNode }

export type SegmentedControlProps = (
	| ToggleGroup.ToggleGroupSingleProps
	| ToggleGroup.ToggleGroupMultipleProps
) &
	ChildProp

const SegmentedControl: React.FC<SegmentedControlProps> = ({ children, ...props }) => {
	return (
		<ToggleGroup.Root {...props} className="inline-flex bg-muted p-1 rounded-md gap-x-1">
			{children}
		</ToggleGroup.Root>
	)
}

export type SegmentedControlItemProps = ToggleGroup.ToggleGroupItemProps

const SegmentedControlItem: React.FC<SegmentedControlItemProps> = ({ children, ...props }) => {
	return (
		<ToggleGroup.Item
			{...props}
			className="px-2 py-1 rounded-md text-sm font-medium focus:bg-secondary focus:text-secondary-foreground aria-checked:bg-primary aria-checked:text-primary-foreground cursor-pointer"
		>
			{children}
		</ToggleGroup.Item>
	)
}

export { SegmentedControl, SegmentedControlItem }
