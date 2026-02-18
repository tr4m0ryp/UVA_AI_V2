"use client"

import { useTranslations } from "next-intl"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import { useUser } from "../user-store"

export const SettingsButton = () => {
	const t = useTranslations("Settings")
	const { toggleSettings } = useUser()

	const handleClick = () => {
		toggleSettings(true)
	}

	return (
		<div className="flex flex-col gap-4">
			<Button variant="ghost" className="flex w-full justify-between" onClick={handleClick}>
				{t("settings-btn")}
				<span>→</span>
			</Button>
		</div>
	)
}
