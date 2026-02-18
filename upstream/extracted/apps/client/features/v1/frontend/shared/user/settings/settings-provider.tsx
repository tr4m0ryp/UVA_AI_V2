"use client"

import { usePathname, useRouter } from "next/navigation"
import { useSession } from "next-auth/react"
import { createContext, useContext, useEffect, useState } from "react"
import { toast } from "@/features/v1/frontend/shared/ui/use-toast"
import type { UserSettingKey } from "../models"
import { GetUserSettings, SaveUserSetting } from "../user-data"
import { userStore } from "../user-store"

interface SettingsContextType {
	settings: Map<UserSettingKey, string>
	setSetting: (key: UserSettingKey, value: string) => Promise<void>
	isLoading: boolean
}

const SettingsContext = createContext<SettingsContextType | undefined>(undefined)

export function SettingsProvider({ children }: { children: React.ReactNode }) {
	const [settings, setSettings] = useState<Map<UserSettingKey, string>>(new Map())
	const [isLoading, setIsLoading] = useState(true)
	const { data: session, status: sessionStatus } = useSession()
	const router = useRouter()
	const pathname = usePathname()

	useEffect(() => {
		const loadSettings = async () => {
			setIsLoading(true)
			if (sessionStatus === "authenticated" && session?.user) {
				try {
					const response = await GetUserSettings()
					const settingsMap = new Map()
					response.forEach((setting) => {
						settingsMap.set(setting.settingKey as UserSettingKey, setting.settingValue)
					})
					setSettings(settingsMap)
					userStore.updateSettings(settingsMap)
				} catch (error) {
					toast({
						title: "Error",
						description: `Could not load settings: ${error}`,
						variant: "destructive",
					})
				}
			} else if (sessionStatus === "unauthenticated") {
				// avoid unnecessary navigation loops when already on the login page
				if (pathname && pathname !== "/") {
					router.push("/")
				}
			}
			setIsLoading(false)
		}

		if (sessionStatus !== "loading") {
			loadSettings()
		}

		if (sessionStatus === "loading") {
			setIsLoading(true)
		}
	}, [sessionStatus, session?.user])

	const setSetting = async (key: UserSettingKey, value: string) => {
		try {
			await SaveUserSetting(key, value)
			const newSettingsMap = new Map(settings)
			newSettingsMap.set(key, value)
			setSettings(newSettingsMap)
			userStore.updateSettings(newSettingsMap)
		} catch (error) {
			toast({
				title: "Error",
				description: `Could not save setting: ${error}`,
				variant: "destructive",
			})
		}
	}

	return (
		<SettingsContext.Provider value={{ settings, setSetting, isLoading }}>
			{children}
		</SettingsContext.Provider>
	)
}

export const useSettings = () => {
	const context = useContext(SettingsContext)
	if (context === undefined) {
		throw new Error("useSettings must be used within a SettingsProvider")
	}
	return context
}
