import type { FC, PropsWithChildren } from "react"

interface Props extends PropsWithChildren {
	title: string
}

export const ExtensionGroup: FC<Props> = (props) => {
	return (
		<div className="flex flex-col">
			<div className="text-sm font-semibold text-menu-subheader p-2">{props.title}</div>
			<div className="flex flex-col">{props.children}</div>
		</div>
	)
}
