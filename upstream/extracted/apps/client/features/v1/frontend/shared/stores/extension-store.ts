import { proxy } from "valtio"
import {
	handleCreateExtension,
	handleDeleteExtension,
	handleFindAllExtensionsForCurrentUser,
	handleUpdateExtension,
} from "@/features/v1/backend/extensions/models/extension-entry"
import type {
	EndpointType,
	ExtensionFunctionModel,
	FrontEndExtensionModel,
	HeaderModel,
} from "@/features/v1/backend/extensions/models/models"
import { RedirectToPage } from "@/features/v1/lib/helpers/navigation-helpers"
import { showError } from "@/features/v1/frontend/shared/ui/global-message-store"
import { uniqueId } from "@/features/v1/lib/helpers/util"
import { useSnapshot } from "@/features/v1/lib/utils/valtio-hooks"

interface FormError {
	message: string
}

interface FormState {
	success: boolean
	errors: FormError[]
}

class ExtensionState {
	private defaultModel: FrontEndExtensionModel = {
		id: "",
		name: "",
		description: "",
		executionSteps: "",
		createdAt: new Date(),
		isPublished: false,
		functions: [],
		headers: [
			{
				id: uniqueId(),
				key: "Content-Type",
				value: "application/json",
			},
		],
		isOwned: true,
	}

	public availableExtensions: FrontEndExtensionModel[] = []
	public isOpened: boolean = false
	public isLoading: boolean = false
	public extension: FrontEndExtensionModel = { ...this.defaultModel }
	public formState: FormState = {
		success: false,
		errors: [],
	}

	public isInfoOpened: boolean = false

	public openInfo() {
		this.isInfoOpened = true
	}

	public closeInfo() {
		this.isInfoOpened = false
	}

	public reset() {
		this.extension = { ...this.defaultModel }
		this.isOpened = false
		this.formState = {
			success: false,
			errors: [],
		}
	}

	public newExtension() {
		this.extension = {
			...this.defaultModel,
			headers: [
				{
					id: uniqueId(),
					key: "Content-Type",
					value: "application/json",
				},
			],
		}
		this.isOpened = true
		this.formState = {
			success: false,
			errors: [],
		}
	}

	public editExtension(extension: FrontEndExtensionModel) {
		// Clone the extension to ensure we can modify it (handles readonly snapshots from valtio)
		const editableExtension = {
			...extension,
			functions: extension.functions.map((f) => ({ ...f, isOpen: false })),
			headers: [...extension.headers],
		}

		this.extension = editableExtension
		this.isOpened = true
		this.formState = {
			success: false,
			errors: [],
		}
	}

	public updateExtension(extension: FrontEndExtensionModel) {
		this.extension = {
			...extension,
		}
		this.isOpened = true
	}

	public setOpened(value: boolean) {
		this.isOpened = value
	}

	public setAvailableExtensions(extensions: FrontEndExtensionModel[]) {
		this.availableExtensions = extensions
	}

	public setLoading(isLoading: boolean) {
		this.isLoading = isLoading
	}

	public setFormState(formState: FormState) {
		this.formState = formState
	}

	// Function management
	public addFunction() {
		this.extension.functions = [...this.extension.functions, this.createDefaultFunction()]
	}

	public cloneFunction(functionModel: ExtensionFunctionModel) {
		const clonedFunction = { ...functionModel, id: uniqueId() }
		this.extension.functions = [...this.extension.functions, clonedFunction]
	}

	public removeFunction(id: string) {
		this.extension.functions = this.extension.functions.filter(
			(f: ExtensionFunctionModel) => f.id !== id,
		)
	}

	public toggleFunction(id: string) {
		this.extension.functions.forEach((f: ExtensionFunctionModel) => {
			if (f.id === id) {
				f.isOpen = !f.isOpen
			} else {
				f.isOpen = false
			}
		})
	}

	public updateFunctionCode(id: string, value: string) {
		const functionToUpdate = this.extension.functions.find(
			(f: ExtensionFunctionModel) => f.id === id,
		)
		if (functionToUpdate) {
			functionToUpdate.code = value
		}
	}

	public addEndpointHeader(props: { key: string; value: string }) {
		const { key, value } = props
		this.extension.headers.push({
			id: uniqueId(),
			key,
			value,
		})
	}

	public removeHeader(props: { headerId: string }) {
		const { headerId } = props
		this.extension.headers = this.extension.headers.filter((h: HeaderModel) => h.id !== headerId)
	}

	private createDefaultFunction(): ExtensionFunctionModel {
		const defaultFunction: ExtensionFunctionModel = {
			id: uniqueId(),
			code: "",
			endpoint: "",
			endpointType: "GET" as EndpointType,
			isOpen: false,
		}
		return defaultFunction
	}
}

export const extensionStore = proxy(new ExtensionState())

export const useExtensionState = () => {
	return useSnapshot(extensionStore, {
		sync: true,
	})
}

// Initialize extension data
export const initializeExtensionData = async () => {
	extensionStore.setLoading(true)

	try {
		const response = await handleFindAllExtensionsForCurrentUser()
		extensionStore.setAvailableExtensions(response)
	} catch (error) {
		showError(`Error initializing extensions: ${error}`)
	} finally {
		extensionStore.setLoading(false)
	}
}

/**
 * Deletes an extension by ID and updates state.
 * @param extensionId - The extension's ID to remove
 */
export const removeExtension = async (extensionId: string) => {
	extensionStore.setLoading(true)

	// Save original extensions for potential restore
	const originalExtensions = [...extensionStore.availableExtensions]

	try {
		// Check if extension exists in store
		const extensionToRemove = extensionStore.availableExtensions.find((e) => e.id === extensionId)

		if (!extensionToRemove) {
			throw new Error("Extension not found")
		}

		if (window.location.pathname.endsWith(`/extensions/${extensionId}`)) {
			RedirectToPage("extensions")
		}

		// Optimistically remove the extension from the store
		const filteredExtensions = extensionStore.availableExtensions.filter(
			(e) => e.id !== extensionId,
		)
		extensionStore.setAvailableExtensions(filteredExtensions)

		// Delete extension from the backend
		await handleDeleteExtension(extensionId)
		return true
	} catch (error) {
		showError(`Error deleting extension: ${error}`)
		extensionStore.setAvailableExtensions(originalExtensions)
		return false
	} finally {
		extensionStore.setLoading(false)
	}
}

/**
 * Adds or updates an extension and handles form state for server actions.
 * @param prevState - Previous form state (for useFormState)
 * @param formData - Form data for extension
 */
export const addOrUpdateExtension = async (_prevState: unknown, formData: FormData) => {
	extensionStore.setLoading(true)
	const originalExtensions = [...extensionStore.availableExtensions]

	try {
		const model = FormDataToExtensionModel(formData)
		const isUpdate = model.id && model.id !== ""

		// If updating, optimistically update the store
		if (isUpdate) {
			const updatedExtensions = extensionStore.availableExtensions.map((e) =>
				e.id === model.id ? { ...e, ...model } : e,
			)
			extensionStore.setAvailableExtensions(updatedExtensions)

			await handleUpdateExtension(model)
		} else {
			const tempId = `temp_${Date.now()}`
			const tempExtension = { ...model, id: tempId }
			extensionStore.setAvailableExtensions([tempExtension, ...extensionStore.availableExtensions])

			const response = await handleCreateExtension(model)

			extensionStore.setAvailableExtensions(
				extensionStore.availableExtensions.map((e) =>
					e.id === tempId ? { ...e, ...response } : e,
				),
			)
		}

		// Success - clear form state and close dialog
		extensionStore.setFormState({
			success: true,
			errors: [],
		})
		extensionStore.setOpened(false)

		return {
			success: true,
			errors: [],
		}
	} catch (error) {
		// Rollback optimistic updates
		extensionStore.setAvailableExtensions(originalExtensions)

		// Handle validation errors
		const errorMessage = error instanceof Error ? error.message : "Unknown error occurred"

		const formState = {
			success: false,
			errors: [{ message: errorMessage }],
		}

		extensionStore.setFormState(formState)

		// Return error state for useFormState
		return formState
	} finally {
		extensionStore.setLoading(false)
	}
}

/**
 * Creates a copy of an existing extension.
 * @param extensionId - The ID of the extension to copy
 */
export const copyExtension = async (extensionId: string) => {
	extensionStore.setLoading(true)
	const originalExtensions = [...extensionStore.availableExtensions]

	try {
		const extensionToCopy = extensionStore.availableExtensions.find((e) => e.id === extensionId)

		if (!extensionToCopy) {
			throw new Error("Extension not found")
		}

		// Optimistically add a new extension copy to the store
		const newExtension: FrontEndExtensionModel = {
			...extensionToCopy,
			id: `temp_${Date.now()}`,
			name: `Copy of ${extensionToCopy.name}`,
			createdAt: new Date(),
		}

		extensionStore.setAvailableExtensions([newExtension, ...extensionStore.availableExtensions])
		await handleCreateExtension(newExtension)

		return true
	} catch (error) {
		showError(`Error copying extension: ${error}`)
		extensionStore.setAvailableExtensions(originalExtensions)
		return false
	} finally {
		extensionStore.setLoading(false)
	}
}

/**
 * Converts FormData to a FrontEndExtensionModel.
 * @param formData - Form data to convert to an extension model
 */
export const FormDataToExtensionModel = (formData: FormData): FrontEndExtensionModel => {
	const headerKeys = formData.getAll("header-key[]")
	const headerValues = formData.getAll("header-value[]")
	const headerIds = formData.getAll("header-id[]")

	const headers: Array<HeaderModel> = headerKeys.map((k, index) => {
		return {
			id: headerIds[index] as string,
			key: k as string,
			value: headerValues[index] as string,
		}
	})

	const endpointTypes = formData.getAll("endpoint-type[]")
	const endpoints = formData.getAll("endpoint[]")
	const codes = formData.getAll("code[]")
	const functions: Array<ExtensionFunctionModel> = endpointTypes.map((endpointType, index) => {
		return {
			id: uniqueId(),
			endpointType: endpointType as EndpointType,
			endpoint: endpoints[index] as string,
			code: codes[index] as string,
			isOpen: false,
		}
	})

	return {
		id: formData.get("id") as string,
		name: formData.get("name") as string,
		description: formData.get("description") as string,
		executionSteps: formData.get("executionSteps") as string,
		isPublished: formData.get("isPublished") === "on",
		createdAt: new Date(),
		functions: functions,
		headers: headers,
		isOwned: true,
	}
}

// Legacy aliases for backward compatibility
export const AddOrUpdateExtension = addOrUpdateExtension
export const FormToExtensionModel = FormDataToExtensionModel

export const exampleFunction = `{
  "name": "UpdateGitHubIssue",
  "parameters": {
    "type": "object",
    "properties": {
      "query": {
        "type": "object",
        "description": "Query parameters",
        "properties": {
          "ISSUE_NUMBER": {
            "type": "string",
            "description": "Github issue number",
            "example": "123"
          }
        },
        "example": {
          "ISSUE_NUMBER": "123"
        },
        "required": ["ISSUE_NUMBER"]
      },
      "body": {
        "type": "object",
        "description": "Body of the GitHub issue",
        "properties": {
          "title": {
            "type": "string",
            "description": "Title of the issue",
            "example": "I'm having a problem with this."
          },
          "body": {
            "type": "string",
            "description": "Body of the issue",
            "example": "I'm having a problem with this."
          },
          "labels": {
            "type": "array",
            "description": "Labels to add to the issue",
            "items": {
              "type": "string",
              "example": "bug"
            }
          }
        },
        "example": {
          "title": "I'm having a problem with this.",
          "body": "I'm having a problem with this.",
          "labels": ["bug"]
        },
        "required": ["title"]
      }
    },
    "required": ["body", "query"]
  },
  "description": "You must use this to only update an existing GitHub issue"
}`
