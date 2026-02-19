"use client"
import { useTranslations } from "next-intl"
import { useEffect } from "react"
import { MenuTrayToggle } from "@/features/v1/frontend/shared/components/main-menu/menu-tray-toggle"
import { MaterialSymbol } from "@/features/v1/frontend/shared/ui/material-symbol" // GAI-258: Using React 19 compatible MaterialSymbol wrapper instead of react-material-symbols
import {
	Menu,
	MenuBar,
	MenuItem,
	MenuItemContainer,
	menuIconProps,
} from "@/features/v1/frontend/shared/ui/menu"
import { InstructionsManager } from "@/features/v1/frontend/shared/user/settings/custom-instructions-manager"
import { MemoryManager } from "@/features/v1/frontend/shared/user/settings/memory-manager"
import { SettingsManager } from "@/features/v1/frontend/shared/user/settings/settings-manager"
import { initializeMemory } from "@/features/v1/frontend/shared/user/user-store"
import { initializeProjectData } from "../../stores/project-store"
import { initializeGradingData } from "@/features/v1/frontend/grading/stores/grading-store"
import { HelpItems } from "./help-items"
import { MenuLink } from "./menu-link"
import { initializeMenuData } from "./menu-store"
import { UserProfile } from "./user-profile"

export const MainMenu = () => {
	useEffect(() => {
		initializeMemory()
		initializeMenuData()
		initializeProjectData()
		initializeGradingData()
	}, [])

	const t = useTranslations("MainMenu")

	return (
		<>
			<SettingsManager />
			<MemoryManager />
			<InstructionsManager />
			<Menu>
				<MenuBar>
					<MenuItemContainer>
						<MenuTrayToggle />
						<div className="h-6" />
						{/* spacer ^ */}
						<MenuItem tooltip={t("chats-tooltip")}>
							<MenuLink href="/chat" ariaLabel={t("chats-ariaLabel")} label={t("chats-label")}>
								<MaterialSymbol icon="chat" {...menuIconProps} />
							</MenuLink>
						</MenuItem>
						<MenuItem tooltip={t("personas-tooltip")}>
							<MenuLink
								href="/persona"
								ariaLabel={t("personas-ariaLabel")}
								label={t("personas-label")}
							>
								<MaterialSymbol icon="domino_mask" {...menuIconProps} />
							</MenuLink>
						</MenuItem>
						<MenuItem tooltip={t("projects-tooltip")}>
							<MenuLink
								href="/project"
								ariaLabel={t("projects-ariaLabel")}
								label={t("projects-label")}
							>
								<MaterialSymbol icon="folder_open" {...menuIconProps} />
							</MenuLink>
						</MenuItem>
						<MenuItem tooltip={t("groups-tooltip")}>
							<MenuLink href="/groups" ariaLabel={t("groups-ariaLabel")} label={t("groups-label")}>
								<MaterialSymbol icon="group" {...menuIconProps} />
							</MenuLink>
						</MenuItem>
						<MenuItem tooltip={t("prompts-tooltip")}>
							<MenuLink
								href="/prompt"
								ariaLabel={t("prompts-ariaLabel")}
								label={t("prompts-label")}
							>
								<MaterialSymbol icon="book_2" {...menuIconProps} />
							</MenuLink>
						</MenuItem>
						<MenuItem tooltip={t("extensions-tooltip")}>
							<MenuLink
								href="/extensions"
								ariaLabel={t("extensions-ariaLabel")}
								label={t("extensions-label")}
							>
								<MaterialSymbol icon="extension" {...menuIconProps} />
							</MenuLink>
						</MenuItem>
						<MenuItem tooltip="Grading">
							<MenuLink href="/grading" ariaLabel="Grading" label="Grading">
								<MaterialSymbol icon="grading" {...menuIconProps} />
							</MenuLink>
						</MenuItem>
					</MenuItemContainer>
					<MenuItemContainer>
						<MenuItem tooltip={t("support-tooltip")} asChild>
							<HelpItems />
						</MenuItem>
						<MenuItem tooltip={t("profile-tooltip")} asChild>
							<UserProfile />
						</MenuItem>
					</MenuItemContainer>
				</MenuBar>
			</Menu>
		</>
	)
}
