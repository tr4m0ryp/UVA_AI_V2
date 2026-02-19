"use client"
import { useTranslations } from "next-intl"
import type { FrontEndExtensionModel } from "@/features/v1/backend/extensions/models/models"
import {
	extensionStore,
	useExtensionState,
} from "@/features/v1/frontend/shared/stores/extension-store"
import { MenuCTA } from "@/features/v1/frontend/shared/ui/menu-cta"
import { ExtensionGroup } from "./extension-group"
import { ExtensionMenuItem } from "./extension-menu-item"

const ExtensionMenu = () => {
	const { availableExtensions } = useExtensionState()
	const t = useTranslations("Extensions.extension-menu")

	return (
		<div className="px-3 py-6 flex flex-col gap-4 overflow-hidden" data-testid="extension-menu">
			<MenuCTA

				label={t("new-extension-btn")}

				onAction={() => extensionStore.newExtension()}
				data-testid="new-extension-button"
			/>

			<ExtensionGroup title={t("all-extensions-text")} data-testid="all-extensions">
				{availableExtensions.map((item: FrontEndExtensionModel) => (
					<ExtensionMenuItem key={item.id} extension={item} />
				))}
			</ExtensionGroup>
		</div>
	)
}

export default ExtensionMenu
