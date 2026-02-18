"use client"

import { Bell, FlaskConical, Settings, User, UserCog } from "lucide-react"
import { useTranslations } from "next-intl"
import { useEffect, useId, useState } from "react"
import { DeleteAllChatThreads } from "@/features/v1/frontend/chat/components/menu/chat-menu-service"
import { LanguageSwitch } from "@/features/v1/frontend/shared/components/main-menu/language-switch"
import { ThemeToggle } from "@/features/v1/frontend/shared/components/main-menu/theme-toggle"
import { DEFAULT_MODEL } from "@/features/v1/frontend/shared/theme/theme-config"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import {
	Dialog,
	DialogContent,
	DialogHeader,
	DialogTitle,
} from "@/features/v1/frontend/shared/ui/dialog"
import { showError } from "@/features/v1/frontend/shared/ui/global-message-store"
import { Label } from "@/features/v1/frontend/shared/ui/label"
import { cn } from "@/features/v1/frontend/shared/ui/lib"
import {
	SegmentedControl,
	SegmentedControlItem,
} from "@/features/v1/frontend/shared/ui/segmented-control"
import {
	Select,
	SelectContent,
	SelectItem,
	SelectTrigger,
	SelectValue,
} from "@/features/v1/frontend/shared/ui/select"
import { Switch } from "@/features/v1/frontend/shared/ui/switch"
import { toast } from "@/features/v1/frontend/shared/ui/use-toast"
import { UserSettingKey } from "../models"
import { userStore, useUser } from "../user-store"
import DeleteAccountDialog from "./delete-account-dialog"
import { usePreferredModel } from "./preferred-model-store"
import { useSettings } from "./settings-provider"

type SettingsTab =
	| "general"
	| "account"
	| "notifications"
	| "personalization"
	| "speech"
	| "data"
	| "connected-apps"
	| "security"
	| "beta"

export const SettingsManager = () => {
	const t = useTranslations("Settings")
	const { isSettingsOpen, toggleSettings, toggleMemoryManager, toggleInstructionsManager } =
		useUser()
	const [activeTab, setActiveTab] = useState<SettingsTab>("general")
	const { settings, setSetting, isLoading } = useSettings()
	const [isDeleteAccountOpen, setIsDeleteAccountOpen] = useState(false)
	const { preferredModel, models, isLoading: isLoadingModels, setPreferredModel } = usePreferredModel()

	// Generate unique IDs for form elements
	const defaultModelId = useId()
	const promptSuggestionsId = useId()
	const toastNotificationsId = useId()
	const memoryContextId = useId()
	const memoryCreationId = useId()
	const conversationStyleId = useId()
	const customInstructionsId = useId()
	const artifactsEnabledId = useId()
	const autoOpenArtifactsId = useId()
	const codeExecutionEnabledId = useId()

	useEffect(() => {
		if (!isLoading && settings) {
			userStore.updateSettings(settings)
		}
	}, [settings, isLoading])

	const handleOpenChange = (open: boolean) => {
		toggleSettings(open)

		/*     if (!open) {
      // Wait for the animation to complete before forcing cleanup
      setTimeout(() => {
        const overlays = document.querySelectorAll('[data-radix-dialog-overlay]');
        overlays.forEach(overlay => {
          if (overlay.getAttribute('data-state') === 'closed') {
            overlay.remove();
          }
        });
        
        // Also check for any lingering portal containers
        const portals = document.querySelectorAll('[data-radix-portal]');
        portals.forEach(portal => {
          if (!portal.querySelector('[data-state="open"]')) {
            portal.remove();
          }
        });
      }, 300);
    } */
	}

	const handleDeleteAllChats = async () => {
		if (window.confirm(t("delete-chats-confirm-text"))) {
			try {
				await DeleteAllChatThreads()
				toast({
					title: t("delete-chats-title"),
					description: t("delete-chats-success-text"),
				})
			} catch (e) {
				showError(e instanceof Error ? e.message : t("delete-chats-error-text"))
			}
		}
	}
	//Onderste twee zijn voor future releases
	const navigationItems = [
		{ id: "general", label: t("general-btn"), icon: <Settings size={16} /> },
		{ id: "account", label: t("account-btn"), icon: <User size={16} /> },
		{ id: "notifications", label: t("notif-btn"), icon: <Bell size={16} /> },
		{ id: "personalization", label: t("personalization-btn"), icon: <UserCog size={16} /> },
		// { id: 'speech', label: 'Speech', icon: <Mic size={16} /> },
		{ id: "beta", label: t("bf-btn"), icon: <FlaskConical size={16} /> },

		/*     { id: 'data', label: 'Data', icon: <BarChart size={16} /> },
    { id: 'security', label: 'Security', icon: <Lock size={16} /> }, */
	] as const

	return (
		<Dialog open={isSettingsOpen} onOpenChange={handleOpenChange}>
			<DialogContent className="sm:max-w-[800px] w-[calc(100%-2rem)] max-h-[85vh] p-0 bg-background border-border">
				<DialogHeader className="p-6 pb-0">
					<DialogTitle className="text-xl font-normal text-foreground">{t("title")}</DialogTitle>
				</DialogHeader>
				<div className="flex h-[calc(min(50vh-120px,600px))] min-h-[300px] max-h-[600px] border-t">
					<div className="w-[200px] min-w-[160px] border-r border-border overflow-y-auto">
						{navigationItems.map((item) => (
							<button
								type="button"
								key={item.id}
								onClick={() => setActiveTab(item.id)}
								className={cn(
									"w-full text-left px-4 py-2 hover:bg-accent/50 transition-colors cursor-pointer",
									activeTab === item.id && "bg-accent",
								)}
							>
								<span className="flex items-center gap-2">
									<span>{item.icon}</span>
									<span>{item.label}</span>
								</span>
							</button>
						))}
					</div>

					<div className="flex-1 p-6 overflow-y-auto">
						{isLoading ? (
							<div>{t("loading-text")}</div>
						) : (
							<div>
								{activeTab === "general" && (
									<div className="flex flex-col gap-y-6">
										<div className="flex flex-col gap-y-2">
											<h3 className="text-sm font-medium">{t("theme-text")}</h3>
											<ThemeToggle />
										</div>

										{/*                 <div className="flex flex-col gap-y-2">
                  <h3 className="text-sm font-medium">Language</h3>
                  <select
                    className="w-full p-2 rounded-md border border-border bg-background"
                    value={settings.get(UserSettingKey.LANGUAGE) || 'en-US'}
                    onChange={(e) => setSetting(UserSettingKey.LANGUAGE, e.target.value)}
                  >
                    <option value="en-US">English (US)</option>
                    <option value="nl">Nederlands</option>
                  </select>
                </div> */}

										<LanguageSwitch />

										<div className="flex flex-col gap-y-2">
											<div className="flex flex-col gap-y-0.5">
												<Label htmlFor={defaultModelId}>{t("default-model")}</Label>
												<p className="text-sm text-muted-foreground">{t("default-model-text")}</p>
											</div>
											<Select
												value={preferredModel || "system-default"}
												onValueChange={(value) =>
													setPreferredModel(value === "system-default" ? undefined : value)
												}
												disabled={isLoadingModels}
											>
												<SelectTrigger id={defaultModelId} className="w-full">
													<SelectValue placeholder={t("default-model-system")} />
												</SelectTrigger>
												<SelectContent>
													<SelectItem value="system-default">
														{t("default-model-system")} ({DEFAULT_MODEL})
													</SelectItem>
													{models.map((model) => (
														<SelectItem key={model.id} value={model.id}>
															{model.name}
														</SelectItem>
													))}
												</SelectContent>
											</Select>
										</div>

										<div className="flex items-center justify-between gap-x-2">
											<div className="flex flex-col gap-y-0.5">
												<Label htmlFor={promptSuggestionsId}>{t("prompt-suggestions-label")}</Label>
												<p className="text-sm text-muted-foreground">{t("prompt-disable-text")}</p>
											</div>
											<Switch
												id={promptSuggestionsId}
												checked={
													settings.get(UserSettingKey.IS_PROMPT_SUGGESTIONS_DISABLED) === "true"
												}
												onCheckedChange={(checked) =>
													setSetting(
														UserSettingKey.IS_PROMPT_SUGGESTIONS_DISABLED,
														checked.toString(),
													)
												}
											/>
										</div>

										<div className="flex flex-col gap-y-2">
											<Button
												variant="destructive"
												className="w-full"
												onClick={() => {
													handleDeleteAllChats()
												}}
											>
												{t("delete-chats-btn")}
											</Button>
										</div>
									</div>
								)}

								{activeTab === "account" && (
									<div className="flex flex-col gap-y-6">
										<div className="flex flex-col gap-y-2">
											<h3 className="text-sm font-medium">{t("account-label")}</h3>
											<p className="text-sm text-muted-foreground">{t("account-text")}</p>
											<Button
												variant="destructive"
												className="w-full"
												onClick={() => setIsDeleteAccountOpen(true)}
											>
												{t("delete-account-btn")}
											</Button>
										</div>

										<DeleteAccountDialog
											open={isDeleteAccountOpen}
											onOpenChange={setIsDeleteAccountOpen}
											onDeleted={() => toggleSettings(false)}
										/>
									</div>
								)}

								{activeTab === "notifications" && (
									<div className="flex flex-col gap-y-6">
										<div className="flex items-center justify-between gap-x-2">
											<div className="flex flex-col gap-y-0.5">
												<Label htmlFor={toastNotificationsId}>{t("notif-title")}</Label>
												<p className="text-sm text-muted-foreground">{t("notif-text")}</p>
											</div>
											<Switch
												id={toastNotificationsId}
												checked={
													settings.get(UserSettingKey.TOAST_NOTIFICATIONS_ENABLED) === "true"
												}
												onCheckedChange={(checked) =>
													setSetting(UserSettingKey.TOAST_NOTIFICATIONS_ENABLED, checked.toString())
												}
											/>
										</div>
									</div>
								)}

								{activeTab === "personalization" && (
									<div className="flex flex-col gap-y-6">
										<div className="flex items-center justify-between gap-x-2">
											<div className="flex flex-col gap-y-0.5">
												<Label htmlFor={memoryContextId}>{t("memory-context-label")}</Label>
												<p className="text-sm text-muted-foreground">{t("memory-context-text")}</p>
											</div>
											<Switch
												id={memoryContextId}
												checked={settings.get(UserSettingKey.IS_MEMORY_CONTEXT_ENABLED) === "true"}
												onCheckedChange={(checked) =>
													setSetting(UserSettingKey.IS_MEMORY_CONTEXT_ENABLED, checked.toString())
												}
											/>
										</div>

										<div className="flex items-center justify-between gap-x-2 opacity-60">
											<div className="flex flex-col gap-y-0.5">
												<Label htmlFor={memoryCreationId}>{t("memory-creation-label")}</Label>
												<p className="text-sm text-muted-foreground">{t("memory-creation-text")}</p>
											</div>
											<Switch id={memoryCreationId} disabled={true} checked={false} />
										</div>

										<div className="flex flex-col gap-y-2">
											<Button
												variant="outline"
												onClick={() => toggleMemoryManager(true)}
												disabled={isLoading}
											>
												{t("memory-mgmt-btn")}
											</Button>
										</div>

										<div className="border-t"></div>

										<div className="flex items-center justify-between gap-x-2">
											<div className="flex flex-col gap-y-0.5">
												<Label htmlFor={conversationStyleId}>{t("conv-style-label")}</Label>
												<p className="text-sm text-muted-foreground">{t("conv-style-text")}</p>
											</div>
											<SegmentedControl
												id={conversationStyleId}
												type="single"
												defaultValue={settings.get(UserSettingKey.CONVERSATION_STYLE)}
												onValueChange={(value) => {
													setSetting(UserSettingKey.CONVERSATION_STYLE, value)
												}}
											>
												<SegmentedControlItem value="concise">{t("btn-1")}</SegmentedControlItem>
												<SegmentedControlItem value="normal">{t("btn-2")}</SegmentedControlItem>
												<SegmentedControlItem value="verbose">{t("btn-3")}</SegmentedControlItem>
											</SegmentedControl>
										</div>

										<div className="border-t"></div>

										<div className="flex items-center justify-between gap-x-2">
											<div className="flex flex-col gap-y-0.5">
												<Label htmlFor={customInstructionsId}>{t("use-custom-instr-label")}</Label>
												<p className="text-sm text-muted-foreground">
													{t("use-custom-instr-text")}
												</p>
											</div>
											<Switch
												id={customInstructionsId}
												checked={
													settings.get(UserSettingKey.IS_CUSTOM_SYSTEM_PROMPT_ACTIVATED) === "true"
												}
												onCheckedChange={(checked: boolean) =>
													setSetting(
														UserSettingKey.IS_CUSTOM_SYSTEM_PROMPT_ACTIVATED,
														checked.toString(),
													)
												}
											/>
										</div>

										<div className="flex flex-col gap-y-2">
											<h3 className="text-sm font-medium">{t("custom-instr-label")}</h3>
											<p className="text-sm text-muted-foreground">{t("custom-instr-text")}</p>
											<Button
												variant="outline"
												onClick={() => toggleInstructionsManager(true)}
												disabled={isLoading}
											>
												{t("edit-btn")}
											</Button>
										</div>

										<div className="border-t"></div>
{/* 
										<div className="flex items-center justify-between gap-x-2">
											<div className="flex flex-col gap-y-0.5">
												<Label htmlFor={autoRegenerateId}>{t("regenerate-msg-label")}</Label>
												<p className="text-sm text-muted-foreground">{t("regenerate-msg-text")}</p>
											</div>
											<Switch
												id={autoRegenerateId}
												checked={!(settings.get(UserSettingKey.REGENERATE_AFTER_EDIT) === "false")}
												onCheckedChange={(checked) =>
													setSetting(UserSettingKey.REGENERATE_AFTER_EDIT, checked.toString())
												}
											/>
										</div> */}
									</div>
								)}

								{/* {activeTab === 'speech' && (
              <div className="flex flex-col gap-y-6">
                <div className="flex flex-col gap-y-2">

                  <div className="flex items-center justify-between gap-x-2">
                  <div className="flex flex-col gap-y-0.5">
                    <Label htmlFor='speech-enabled'>Speech (DEMO) </Label>
                    <p className='text-sm text-muted-foreground'>
                      Have standard narration turned on or off
                    </p>
                  </div>
                    <Switch
                      id="speech-enabled"
                      checked={settings.get(UserSettingKey.SPEECH_ENABLED) === 'true'}
                      onCheckedChange={(checked) =>
                        setSetting(UserSettingKey.SPEECH_ENABLED, checked.toString())
                      }
                    />
                  </div>

                </div>
              </div>
            )} */}

								{activeTab === "beta" && (
									<div className="flex flex-col gap-y-6">
										<div className="flex flex-col gap-y-2">
											<div className="flex items-center justify-between gap-x-2">
												<div className="flex flex-col gap-y-0.5">
													<Label htmlFor={artifactsEnabledId}>{t("artifacts-label")}</Label>
													<p className="text-sm text-muted-foreground">{t("artifacts-text")}</p>
												</div>
												<Switch
													id={artifactsEnabledId}
													checked={!(settings.get(UserSettingKey.ARTIFACTS_ENABLED) === "false")}
													onCheckedChange={(checked) =>
														setSetting(UserSettingKey.ARTIFACTS_ENABLED, checked.toString())
													}
												/>
											</div>
										</div>
										<div className="flex flex-col gap-y-2">
											<div className="flex items-center justify-between gap-x-2">
												<div className="flex flex-col gap-y-0.5">
													<Label htmlFor={autoOpenArtifactsId}>{t("auto-artifacts-label")}</Label>
													<p className="text-sm text-muted-foreground">
														{t("auto-artifacts-text")}
													</p>
												</div>
												<Switch
													id={autoOpenArtifactsId}
													checked={!(settings.get(UserSettingKey.AUTO_OPEN_ARTIFACTS) === "false")}
													onCheckedChange={(checked) =>
														setSetting(UserSettingKey.AUTO_OPEN_ARTIFACTS, checked.toString())
													}
												/>
											</div>
										</div>
										<div className="flex flex-col gap-y-2">
											<div className="flex items-center justify-between gap-x-2">
												<div className="flex flex-col gap-y-0.5">
													<Label htmlFor={codeExecutionEnabledId}>
														{t("code-execution-label")}
													</Label>
													<p className="text-sm text-muted-foreground">
														{t("code-execution-text")}
													</p>
												</div>
												<Switch
													id={codeExecutionEnabledId}
													checked={
														!(settings.get(UserSettingKey.CODE_EXECUTION_ENABLED) === "false")
													}
													onCheckedChange={(checked) =>
														setSetting(UserSettingKey.CODE_EXECUTION_ENABLED, checked.toString())
													}
												/>
											</div>
										</div>
									</div>
								)}

								{/*             {activeTab === 'data' && (
              <div className="flex flex-col gap-y-6">
                <div className="flex flex-col gap-y-2">

                  <div className="flex items-center justify-between gap-x-2">
                  <div className="flex flex-col gap-y-0.5">
                    <Label htmlFor='memory-creation-enabled'>A/B Testing</Label>
                    <p className='text-sm text-muted-foreground'>
                      Allow A/B testing of responses. (Your data is not shared with anyone and will not be used for training)
                    </p>
                  </div>
                    <Switch
                      id="ab-testing-enabled"
                      checked={settings.get(UserSettingKey.IS_AB_TESTING_ENABLED) === 'true'}
                      onCheckedChange={(checked) =>
                        setSetting(UserSettingKey.IS_AB_TESTING_ENABLED, checked.toString())
                      }
                    />
                </div>


                </div>
              </div>
            )} */}

								{/*             {activeTab === 'security' && (
              <div className="flex flex-col gap-y-6">
                <div className="flex flex-col gap-y-2">
                  <h3 className="text-sm font-medium">Security</h3>
                </div>
              </div>
            )} */}
							</div>
						)}
					</div>
				</div>
			</DialogContent>
		</Dialog>
	)
}
