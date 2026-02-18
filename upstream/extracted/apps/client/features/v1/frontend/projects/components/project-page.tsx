"use client"

export interface ProjectChild {
	id: string
	name: string
	type?: string
	children?: ProjectChild[]
	parentId?: string | null
	[key: string]: unknown
}

export type ProjectNode = ProjectChild & {
	children: ProjectChild[]
	parentId?: string | null
}
