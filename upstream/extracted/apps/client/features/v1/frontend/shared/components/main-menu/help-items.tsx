"use client"

import { Download, ExternalLink, HelpCircle } from "lucide-react"
import { useTranslations } from "next-intl"

import Image from "next/image"

import { useState } from "react"
import {
	DropdownMenu,
	DropdownMenuContent,
	DropdownMenuItem,
	DropdownMenuPortal,
	DropdownMenuSeparator,
	DropdownMenuSub,
	DropdownMenuSubContent,
	DropdownMenuSubTrigger,
	DropdownMenuTrigger,
} from "@/features/v1/frontend/shared/ui/dropdown-menu"
import { Button, mainMenuVariant } from "../../ui/button"
import { cn } from "../../ui/lib"
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from "../../ui/tooltip"

export const HelpItems = () => {
	const [_disclaimerDialogOpen, _setDisclaimerDialogOpen] = useState<boolean>(false)

	const t = useTranslations("MainMenu.support")

	return (
		<DropdownMenu>
			<TooltipProvider delayDuration={0}>
				<Tooltip>
					<TooltipTrigger asChild>
						<DropdownMenuTrigger asChild>
							<Button variant="mainMenu" aria-label="Help menu" className={cn(mainMenuVariant)}>
								<HelpCircle size={32} strokeWidth={1.5} />
							</Button>
						</DropdownMenuTrigger>
					</TooltipTrigger>
					<TooltipContent
						align="center"
						side="right"
						sideOffset={8}
						className="p-0 border-0 bg-transparent shadow-none"
					>
						<div className="bg-main-menu-tooltip-background ml-main-menu-tooltip text-main-menu-tooltip-foreground font-light  rounded-sm px-3 py-1.5 whitespace-nowrap relative ">
							{t("support-feedback-btn")}
						</div>
					</TooltipContent>
				</Tooltip>
			</TooltipProvider>
			<DropdownMenuContent side="right" className="w-64" align="end">
				<DropdownMenuSub>
					<DropdownMenuSubTrigger>
						<span>{t("getting-started-btn")}</span>
					</DropdownMenuSubTrigger>
					<DropdownMenuPortal>
						<DropdownMenuSubContent className="w-48">
							<DropdownMenuItem asChild>
								<a
									href="https://edu.nl/r4duh"
									target="_blank"
									rel="noopener noreferrer"
									className="flex justify-between items-center"
								>

									<span className="flex items-center gap-2">
										<Image src="/flags/en.svg" alt="English" width={16} height={16} className="rounded-sm" />
										{t("en-course")}
									</span>

									<ExternalLink size={16} />
								</a>
							</DropdownMenuItem>
							<DropdownMenuItem asChild>
								<a
									href="https://edu.nl/u97mn"
									target="_blank"
									rel="noopener noreferrer"
									className="flex justify-between items-center"
								>


									<span className="flex items-center gap-2">
										<Image src="/flags/nl.svg" alt="Dutch" width={16} height={16} className="rounded-sm" />
										{t("nl-course")}
									</span>

									<ExternalLink size={16} />
								</a>
							</DropdownMenuItem>
						</DropdownMenuSubContent>
					</DropdownMenuPortal>
				</DropdownMenuSub>

				<DropdownMenuSub>
					<DropdownMenuSubTrigger>
						<span>{t("ai-literacy-btn")}</span>
					</DropdownMenuSubTrigger>
					<DropdownMenuPortal>
						<DropdownMenuSubContent className="w-48">
							<DropdownMenuItem asChild>
								<a
									href="https://edu.nl/r47pa"
									target="_blank"
									rel="noopener noreferrer"
									className="flex justify-between items-center"
								>

									<span className="flex items-center gap-2">
										<Image src="/flags/en.svg" alt="English" width={16} height={16} className="rounded-sm" />
										{t("en-teachers")}
									</span>

									<ExternalLink size={16} />
								</a>
							</DropdownMenuItem>
							<DropdownMenuItem asChild>
								<a
									href="https://edu.nl/qdqqu"
									target="_blank"
									rel="noopener noreferrer"
									className="flex justify-between items-center"
								>

									<span className="flex items-center gap-2">
										<Image src="/flags/nl.svg" alt="Dutch" width={16} height={16} className="rounded-sm" />
										{t("nl-teachers")}
									</span>

									<ExternalLink size={16} />
								</a>
							</DropdownMenuItem>
							<DropdownMenuItem asChild>
								<a
									href="https://edu.nl/gakhm"
									target="_blank"
									rel="noopener noreferrer"
									className="flex justify-between items-center"
								>

									<span className="flex items-center gap-2">
										<Image src="/flags/en.svg" alt="English" width={16} height={16} className="rounded-sm" />
										{t("en-students")}
									</span>

									<ExternalLink size={16} />
								</a>
							</DropdownMenuItem>
							<DropdownMenuItem asChild>
								<a
									href="https://edu.nl/mgmvu"
									target="_blank"
									rel="noopener noreferrer"
									className="flex justify-between items-center"
								>

									<span className="flex items-center gap-2">
										<Image src="/flags/nl.svg" alt="Dutch" width={16} height={16} className="rounded-sm" />
										{t("nl-students")}
									</span>

									<ExternalLink size={16} />
								</a>
							</DropdownMenuItem>
						</DropdownMenuSubContent>
					</DropdownMenuPortal>
				</DropdownMenuSub>

				<DropdownMenuSub>
					<DropdownMenuSubTrigger>
						<span>{t("user-manual-btn")}</span>
					</DropdownMenuSubTrigger>
					<DropdownMenuPortal>
						<DropdownMenuSubContent className="w-60">
							<DropdownMenuItem asChild>
								<a
									href="https://edu.nl/wg8v4"
									target="_blank"
									rel="noopener noreferrer"
									className="flex justify-between items-center"
								>

									<span className="flex items-center gap-2">
										<Image src="/flags/en.svg" alt="English" width={16} height={16} className="rounded-sm" />
										{t("en-pdf")}
									</span>

									<ExternalLink size={16} />
								</a>
							</DropdownMenuItem>
							<DropdownMenuItem asChild>
								<a
									href="https://edu.nl/9kfww"
									target="_blank"
									rel="noopener noreferrer"
									className="flex justify-between items-center"
								>

									<span className="flex items-center gap-2">
										<Image src="/flags/nl.svg" alt="Dutch" width={16} height={16} className="rounded-sm" />
										{t("nl-pdf")}
									</span>

									<ExternalLink size={16} />
								</a>
							</DropdownMenuItem>
						</DropdownMenuSubContent>
					</DropdownMenuPortal>
				</DropdownMenuSub>

				<DropdownMenuSeparator />

				<DropdownMenuItem asChild>
					<a
						href="https://www.uva.nl/binaries/content/assets/uva/nl/over-de-uva/over-de-uva/ai-in-het-onderwijs/disclaimer-uva-ai-chat-30-juni-2025-nederlands.pdf"
						target="_blank"
						rel="noopener noreferrer"
						className="flex justify-between"
					>
						{t("disclaimer-btn")}
						<Download size={16} />
					</a>
				</DropdownMenuItem>
				<DropdownMenuSeparator />
				<DropdownMenuItem asChild>
					<a
						href="https://forms.office.com/e/9RsU616s6s"
						target="_blank"
						rel="noopener noreferrer"
						className="flex justify-between"
					>
						{t("feedback-form-btn")}
						<ExternalLink size={16} />
					</a>
				</DropdownMenuItem>
			</DropdownMenuContent>
		</DropdownMenu>
	)
}
