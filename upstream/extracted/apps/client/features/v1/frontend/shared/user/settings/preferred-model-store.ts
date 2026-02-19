"use client"

import { useEffect } from "react"
import { proxy } from "valtio"
import { useSnapshot } from "@/features/v1/lib/utils/valtio-hooks"
import {
	fetchAvailableModelsAction,
	type ModelOption,
} from "@/features/v1/lib/services/model-service"
import { showError } from "@/features/v1/frontend/shared/ui/global-message-store"

const STORAGE_KEY = "preferred-model"

class PreferredModelState {
	public preferredModel: string | undefined = undefined
	public models: ModelOption[] = []
	public isLoading: boolean = true
	private isInitialized: boolean = false

	public async initialize() {
		if (this.isInitialized) return
		this.isInitialized = true

		// Load from localStorage
		if (typeof window !== "undefined") {
			try {
				preferredModelStore.preferredModel = localStorage.getItem(STORAGE_KEY) || undefined
			} catch {
				console.error("Failed to load preferred model")
			}
		}

		// Fetch available models
		try {
			preferredModelStore.models = await fetchAvailableModelsAction()

			// check that the model exists
			if (preferredModelStore.preferredModel) {
				const modelExists = preferredModelStore.models.some(
					(model) => model.id === preferredModelStore.preferredModel
				)
				if (!modelExists) {
					showError(
						`Your preferred model "${preferredModelStore.preferredModel}" is no longer available. You can select a new preferred model in settings.`
					)
					preferredModelStore.setPreferredModel(undefined)
				}
			}
		} catch {
			console.error("Failed to fetch models")
		} finally {
			preferredModelStore.isLoading = false
		}
	}

	public setPreferredModel(modelId: string | undefined) {
		preferredModelStore.preferredModel = modelId

		if (typeof window === "undefined") return
		try {
			if (modelId) {
				localStorage.setItem(STORAGE_KEY, modelId)
			} else {
				localStorage.removeItem(STORAGE_KEY)
			}
		} catch {
			console.error("Failed to save preferred model")
		}
	}
}

export const preferredModelStore = proxy(new PreferredModelState())

export const usePreferredModel = () => {
	const snapshot = useSnapshot(preferredModelStore)

	// Initialize on mount
	useEffect(() => {
		preferredModelStore.initialize()
	}, [])

	return {
		preferredModel: snapshot.preferredModel,
		models: snapshot.models,
		isLoading: snapshot.isLoading,
		setPreferredModel: preferredModelStore.setPreferredModel.bind(preferredModelStore),
	}
}
