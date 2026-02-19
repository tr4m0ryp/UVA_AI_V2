"use client"

import { signOut, useSession } from "next-auth/react"
import { useTranslations } from "next-intl"
import { Avatar, AvatarImage } from "@/features/v1/frontend/shared/ui/avatar"
import {
	DropdownMenu,
	DropdownMenuContent,
	DropdownMenuItem,
	DropdownMenuLabel,
	DropdownMenuSeparator,
	DropdownMenuTrigger,
} from "@/features/v1/frontend/shared/ui/dropdown-menu"
import { MaterialSymbol } from "@/features/v1/frontend/shared/ui/material-symbol" // GAI-258: Using React 19 compatible MaterialSymbol wrapper instead of react-material-symbols
import { menuIconProps } from "@/features/v1/frontend/shared/ui/menu"
import { SettingsButton } from "@/features/v1/frontend/shared/user/settings/settings-button"
import { Button, mainMenuVariant } from "../../ui/button"
import { cn } from "../../ui/lib"
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from "../../ui/tooltip"
import { ThemeToggle } from "./theme-toggle"

export const UserProfile = () => {
	const { data: session } = useSession()
	const t = useTranslations("MainMenu.user-prof")

	return (
		<DropdownMenu modal={false}>
			<TooltipProvider delayDuration={0}>
				<Tooltip>
					<TooltipTrigger asChild>
						<DropdownMenuTrigger asChild>
							{session?.user?.image ? (
								<Avatar className="w-8 h-8">
									<AvatarImage src={session?.user?.image!} alt={session?.user?.name!} />
								</Avatar>
							) : (
								<Button
									variant="mainMenu"
									aria-label={t("user-prof-ariaLabel")}
									className={cn(mainMenuVariant)}
								>
									<MaterialSymbol icon="account_circle" {...menuIconProps} />
								</Button>
							)}
						</DropdownMenuTrigger>
					</TooltipTrigger>
					<TooltipContent
						align="center"
						side="right"
						sideOffset={8}
						className="p-0 border-0 bg-transparent shadow-none"
					>
						<div className="bg-main-menu-tooltip-background ml-main-menu-tooltip text-main-menu-tooltip-foreground font-light  rounded-sm px-3 py-1.5 whitespace-nowrap relative ">
							{t("settings-btn")}
						</div>
					</TooltipContent>
				</Tooltip>
			</TooltipProvider>
			<DropdownMenuContent side="right" className="w-56" align="end">
				<DropdownMenuLabel className="font-normal">
					<div className="flex flex-col gap-2">
						<p className="text-sm font-medium leading-none">{session?.user?.name}</p>
						<p className="text-xs leading-none text-muted-foreground">{session?.user?.email}</p>
						<p className="text-xs leading-none text-muted-foreground"></p>
					</div>
				</DropdownMenuLabel>

				<DropdownMenuSeparator />
				<DropdownMenuLabel className="font-normal">
					<SettingsButton />
				</DropdownMenuLabel>

				<DropdownMenuSeparator />
				<DropdownMenuLabel className="font-normal">
					<div className="flex flex-col gap-2">
						<p className="text-sm font-medium leading-none">{t("switch-themes")}</p>
						<ThemeToggle />
					</div>
				</DropdownMenuLabel>
				<DropdownMenuSeparator />
				{/* <DropdownMenuLabel>
          <div>
            <p className="text-sm font-medium leading-none">Opt out</p>
            <OptOutManager />
          </div>
        </DropdownMenuLabel> */}
				<DropdownMenuSeparator />
				<DropdownMenuItem className="flex gap-2" onClick={() => signOut({ callbackUrl: "/" })}>
					<MaterialSymbol icon="logout" {...menuIconProps} size={18} />
					<span>{t("logout-btn")}</span>
				</DropdownMenuItem>
			</DropdownMenuContent>
		</DropdownMenu>
	)
}
