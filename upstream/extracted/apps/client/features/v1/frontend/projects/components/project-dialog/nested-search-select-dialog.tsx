import { Collapsible, CollapsibleContent, CollapsibleTrigger } from "@radix-ui/react-collapsible"
import { Check, ChevronDown, CornerDownRight } from "lucide-react"
import { useTranslations } from "next-intl"
import React, { type FC, useCallback, useMemo, useState } from "react"
import type { FrontEndProjectModel } from "@/features/v1/backend/projects/models/models"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import {
	Dialog,
	DialogContent,
	DialogHeader,
	DialogTitle,
} from "@/features/v1/frontend/shared/ui/dialog"
import { useDebounce } from "@/features/v1/frontend/shared/ui/hooks/use-debounce"
import { Input } from "@/features/v1/frontend/shared/ui/input"
import { cn } from "@/features/v1/frontend/shared/ui/lib"
import { MaterialSymbol } from "@/features/v1/frontend/shared/ui/material-symbol" // GAI-258: Using React 19 compatible MaterialSymbol wrapper instead of react-material-symbols
import type { ProjectNode } from "../project-page"

interface NestedSearchSelectDialogProps {
	/**
	 * If provided, the component will skip showing the rootProject itself
	 * and any project whose direct parent is rootProject.
	 * If omitted, the component shows all projects.
	 */
	rootProject?: FrontEndProjectModel
	projectMap: Map<string, ProjectNode>
	excludedIds?: Set<string>
	onSelect: (projectId: string) => void
	isOpen: boolean
	onOpenChange: (open: boolean) => void
	title?: string
	searchPlaceholder?: string
	noItemsMessage?: string
}

export const NestedSearchSelectDialog: FC<NestedSearchSelectDialogProps> = ({
	rootProject,
	projectMap,
	excludedIds,
	onSelect,
	isOpen,
	onOpenChange,
	title = "Select a Project",
	searchPlaceholder = "Search...",
	noItemsMessage = "No items found.",
}) => {
	const [selectedProjectId, setSelectedProjectId] = useState<string | null>(null)
	const [searchQuery, setSearchQuery] = useState("")
	const debouncedSearchQuery = useDebounce(searchQuery, 200)
	const t = useTranslations("Projects.nested-project-search")

	const topLevelProjects = useMemo(() => {
		return Array.from(projectMap.values()).filter(
			(project) =>
				!project.parentId && // Is a top-level project in the original map
				(!excludedIds || !excludedIds.has(project.id)), // And is not in the general excluded set
		)
	}, [projectMap, excludedIds])

	const flattenProjects = useCallback(
		(projectNode: ProjectNode): ProjectNode[] => {
			// General exclusion: if this node is in the excludedIds set, skip it and its children entirely.
			if (excludedIds?.has(projectNode.id)) {
				return []
			}

			// Specific exclusion for the project being moved (rootProject) and its direct children.
			// This handles cases where a project might be moved to a sibling, which is valid,
			// but not into itself or its own immediate children.
			if (rootProject) {
				if (projectNode.id === rootProject.id) {
					return [] // Don't include the project being moved itself
				}
				if (projectNode.parentId === rootProject.id) {
					return [] // Don't include direct children of the project being moved as targets
				}
			}

			// Filter children before recursion
			const validChildren = projectNode.children.filter(
				(child): child is ProjectNode =>
					child &&
					typeof child === "object" &&
					"children" in child &&
					"id" in child && // Is a ProjectNode
					(!excludedIds || !excludedIds.has(child.id)), // And is not in the general excluded set
			)

			const subprojects = validChildren.flatMap((childNode: ProjectNode) =>
				flattenProjects(childNode),
			)
			return [projectNode, ...subprojects]
		},
		[rootProject, excludedIds],
	)

	const allProjectsForSearch = useMemo(() => {
		return topLevelProjects.flatMap((pNode) => flattenProjects(pNode))
	}, [topLevelProjects, flattenProjects])

	const filteredProjects = useMemo(() => {
		if (!debouncedSearchQuery) return allProjectsForSearch
		const q = debouncedSearchQuery.toLowerCase()
		return allProjectsForSearch.filter((f: ProjectNode) => f.name.toLowerCase().includes(q))
	}, [allProjectsForSearch, debouncedSearchQuery])

	const renderProjects = useCallback(
		(projectNode: ProjectNode, depth = 0): React.ReactElement | null => {
			// Do not render if this project itself is in the excluded set.
			// (topLevelProjects already filters this, but this is a safeguard if renderProjects is ever called directly with an excluded node)
			if (excludedIds?.has(projectNode.id)) {
				return null
			}

			const subprojects = projectNode.children.filter(
				(child): child is ProjectNode =>
					child &&
					typeof child === "object" &&
					"children" in child &&
					"id" in child && // Is a ProjectNode
					(!excludedIds || !excludedIds.has(child.id)), // And is not generally excluded
			)

			return (
				<div key={projectNode.id} className={cn("flex flex-col gap-y-1", depth > 0 && "ml-4")}>
					<Collapsible>
						<CollapsibleTrigger
							className={cn(
								"w-full flex items-center justify-between p-2 rounded text-left",
								"hover:bg-accent hover:text-accent-foreground",
								"focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2",
								selectedProjectId === projectNode.id && "bg-accent text-accent-foreground",
							)}
							onClick={() => setSelectedProjectId(projectNode.id)}
						>
							<span className="flex items-center gap-x-2 min-w-0">
								<MaterialSymbol icon="folder_open" weight={300} size={18} className="shrink-0" />
								<span className="truncate max-w-xs">{projectNode.name}</span>
							</span>
							{subprojects.length > 0 && (
								<ChevronDown size={16} className="text-muted-foreground shrink-0" />
							)}
						</CollapsibleTrigger>
						{subprojects.length > 0 && (
							<CollapsibleContent>
								<div className="relative pl-2.5">
									<CornerDownRight
										size={14}
										className="absolute top-2 left-2 text-muted-foreground"
									/>
									{subprojects
										.map((subprojectNode: ProjectNode) => renderProjects(subprojectNode, depth + 1))
										.filter(Boolean)}
								</div>
							</CollapsibleContent>
						)}
					</Collapsible>
				</div>
			)
		},
		[selectedProjectId, excludedIds],
	)

	const handleSelect = () => {
		if (selectedProjectId) {
			onSelect(selectedProjectId)
			onOpenChange(false)
			setSelectedProjectId(null)
			setSearchQuery("")
		}
	}

	return (
		<Dialog open={isOpen} onOpenChange={onOpenChange}>
			<DialogContent className="sm:max-w-md" position="top">
				<DialogHeader>
					<DialogTitle>{title}</DialogTitle>
				</DialogHeader>
				<div className="flex flex-col gap-y-4 pt-4">
					<Input
						placeholder={searchPlaceholder}
						value={searchQuery}
						onChange={(e) => setSearchQuery(e.target.value)}
						className="mb-2"
					/>{" "}
					{debouncedSearchQuery ? (
						<div className="max-h-[60vh] overflow-y-auto border rounded">
							{filteredProjects.length === 0 ? (
								<div className="p-4 text-sm text-muted-foreground">{noItemsMessage}</div>
							) : (
								<div className="divide-y">
									{filteredProjects.map((project) => (
										<button
											type="button"
											key={project.id}
											onClick={() =>
												setSelectedProjectId(selectedProjectId === project.id ? null : project.id)
											}
											className={cn(
												"flex w-full items-center px-4 py-2 text-left text-sm hover:bg-accent",
												selectedProjectId === project.id && "bg-accent text-accent-foreground",
											)}
										>
											<Check
												className={cn(
													"mr-2 h-4 w-4 shrink-0",
													selectedProjectId === project.id ? "opacity-100" : "opacity-0",
												)}
											/>
											<span className="truncate max-w-sm">{project.name}</span>
										</button>
									))}
								</div>
							)}
						</div>
					) : (
						<div
							className="max-h-[60vh] overflow-y-auto"
							role="tree"
							aria-label="Project selection"
						>
							{topLevelProjects.map((project) => renderProjects(project)).filter(Boolean)}
						</div>
					)}
				</div>
				<div className="flex flex-row justify-end gap-2 pt-4">
					<Button variant="outline" onClick={() => onOpenChange(false)}>
						{t("cancel-btn")}
					</Button>
					<Button onClick={handleSelect} disabled={!selectedProjectId}>
						{t("select-btn")}
					</Button>
				</div>
			</DialogContent>
		</Dialog>
	)
}
