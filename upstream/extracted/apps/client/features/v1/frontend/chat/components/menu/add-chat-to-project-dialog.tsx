import { NestedSearchSelectDialog } from "@/features/v1/frontend/projects/components/project-dialog/nested-search-select-dialog"
import type { ProjectNode } from "@/features/v1/frontend/projects/components/project-page"
import { useTranslations } from "next-intl"

interface AddChatToProjectDialogProps {
	projectMap: Map<string, ProjectNode>
	onAddToProject: (projectId: string) => Promise<void>
	isOpen: boolean
	onOpenChange: (open: boolean) => void
}


export const AddChatToProjectDialog = ({
	projectMap,
	onAddToProject,
	isOpen,
	onOpenChange,
}: AddChatToProjectDialogProps) => {
	/*   const projectItems = projects.map(project => ({ id: project.id, name: project.name }));
	 */
	 const t = useTranslations ("Chats.dialog")

	return (
		<NestedSearchSelectDialog
			projectMap={projectMap}
			onSelect={(projectId: string) => onAddToProject(projectId)}
			isOpen={isOpen}
			onOpenChange={onOpenChange}
			title= {t("title")}
			searchPlaceholder= {t("search-placeholder")}
			noItemsMessage={t("no-item-msg")}
		/>
		/*     <SearchSelectDialog
      items={projectItems}
      onSubmit={onAddToProject}
      dialogTitle="Add Chat to Project"
      searchPlaceholder="Search projects..."
      noItemsMessage="No projects available."
      successMessage="Chat added to project successfully"
      isOpen={isOpen}
      onOpenChange={onOpenChange}
      showCancelButton
    /> */
	)
}
