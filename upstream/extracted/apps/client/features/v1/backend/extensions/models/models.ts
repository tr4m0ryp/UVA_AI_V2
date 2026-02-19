export type EndpointType = "GET" | "POST" | "PUT" | "DELETE" | "PATCH"

export interface HeaderModel {
	id: string
	key: string
	value: string
}

export interface ExtensionFunctionModel {
	id: string
	name?: string
	code: string
	endpoint: string
	endpointType: EndpointType
	headers?: HeaderModel[]
	isOpen?: boolean
	[key: string]: unknown
}

export interface FrontEndExtensionModel {
	id: string
	name: string
	functions: ExtensionFunctionModel[]
	headers: HeaderModel[]
	[key: string]: unknown
}
