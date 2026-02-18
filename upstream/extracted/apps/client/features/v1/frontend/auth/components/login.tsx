"use client"
import { signIn } from "next-auth/react"
import { useTranslations } from "next-intl"
import { useTheme } from "next-themes"
import type { FC } from "react"
import { AI_NAME, getLogoName, ORG } from "@/features/v1/frontend/shared/theme/theme-config"
import { Avatar, AvatarImage } from "@/features/v1/frontend/shared/ui/avatar"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import {
	Card,
	CardContent,
	CardDescription,
	CardHeader,
	CardTitle,
} from "@/features/v1/frontend/shared/ui/card"

interface LoginProps {
	isDevMode: boolean
}

export const LogIn: FC<LoginProps> = (props) => {
	const isDarkTheme = useTheme().resolvedTheme === "dark"
	const logoName = isDarkTheme ? getLogoName().dark : getLogoName().light

	const t = useTranslations("Login")
	return (
		<Card className="flex gap-2 flex-col min-w-[300px]">
			<CardHeader className="gap-2">
				<CardTitle className="text-2xl flex gap-2">
					<Avatar className="h-8 w-8">
						<AvatarImage src={`/${logoName}`} />
					</Avatar>
					<span className="text-primary">{AI_NAME}</span>
				</CardTitle>
				<CardDescription>
					{t("description1")} {ORG} {t("description2")}
				</CardDescription>
			</CardHeader>
			<CardContent className="grid gap-4">
				<Button onClick={() => signIn("azure-ad")}> {t("login")}</Button>
				{props.isDevMode ? <Button onClick={() => signIn("localdev")}>{t("button")}</Button> : null}
			</CardContent>
		</Card>
	)
}
