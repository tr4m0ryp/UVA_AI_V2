import { cn } from "@/features/v1/frontend/shared/ui/lib"

// add icons here to import them. see available icons: https://fonts.google.com/icons
export const MATERIAL_ICONS = [
	"account_circle",
	"add",
	"attach_file",
	"bookmark",
	"book_2",
	"chat",
	"chat_add_on",
	"domino_mask",
	"edit_document",
	"extension",
	"folder_open",
	"grading",
	"group",
	"image",
	"info",
	"language",
	"left_panel_close",
	"logout",
	"more_vert",
	"neurology",
	"school",
] as const

export type MaterialIconName = (typeof MATERIAL_ICONS)[number]

//build the URL - yes, it only works alphabetically for some reason
export const MATERIAL_SYMBOLS_URL = `https://fonts.googleapis.com/css2?family=Material+Symbols+Outlined:opsz,wght,FILL,GRAD@20..48,100..700,0..1,-50..200&icon_names=${[...MATERIAL_ICONS].sort().join(",")}&display=block`

export interface MaterialSymbolProps extends React.HTMLAttributes<HTMLElement> {
	icon: MaterialIconName
	size?: number
	weight?: number
	fill?: boolean
	className?: string
}

export const MaterialSymbol = ({
	icon,
	size = 24,
	weight = 400,
	fill = false,
	className,
	style,
	...props
}: MaterialSymbolProps) => {
	return (
		<span
			className={cn("material-symbols-outlined", className)}
			style={{
				fontSize: size,
				fontVariationSettings: `'FILL' ${fill ? 1 : 0}, 'wght' ${weight}, 'GRAD' 0, 'opsz' ${size}`,
				...style,
			}}
			{...props}
		>
			{icon}
		</span>
	)
}
