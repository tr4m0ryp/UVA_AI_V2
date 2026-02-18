"use client"

import Image from "next/image"
import { useLocale } from "next-intl"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import { setLocaleAction } from "./language-actions"

const langs = [
	{ code: "en", label: "English", flag: "/flags/en.svg" },
	{ code: "nl", label: "Nederlands", flag: "/flags/nl.svg" },
]

export function LanguageSwitch() {
	const locale = useLocale()

	const handleSwitch = async (newLocale: string) => {
		await setLocaleAction(newLocale)
	}

	return (
		<div className="inline-flex items-center gap-2 p-1 rounded-xl bg-muted border border-border shadow-sm">
			{langs.map((lang) => (
				<Button
					variant="ghost"
					key={lang.code}
					onClick={() => handleSwitch(lang.code)}
					className={`flex items-center gap-4 px-4 py-1.5 rounded-lg text-sm transition-all
						${
							locale === lang.code
								? "bg-red-600 text-white shadow-sm font-medium"
								: "text-foreground/70 hover:bg-blue-100 dark:hover:bg-blue-900/40"
						}
					`}
				>
					<Image src={lang.flag} alt={lang.label} width={22} height={22} className="rounded-sm" />
					{lang.label}
				</Button>
			))}
		</div>
	)
}
