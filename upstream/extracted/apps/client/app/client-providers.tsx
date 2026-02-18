"use client"

import { SessionProvider } from "next-auth/react"
import { ThemeProvider } from "next-themes"
import { SettingsProvider } from "@/features/v1/frontend/shared/user/settings/settings-provider"

export function ClientProviders({ children }: { children: React.ReactNode }) {
	return (
		<SessionProvider>
			<SettingsProvider>
				<ThemeProvider
					attribute="class"
					defaultTheme="light"
					enableSystem
					disableTransitionOnChange
				>
					{children}
				</ThemeProvider>
			</SettingsProvider>
		</SessionProvider>
	)
}
