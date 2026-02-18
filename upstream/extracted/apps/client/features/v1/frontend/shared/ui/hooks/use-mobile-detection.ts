"use client"
import { useEffect, useState } from "react"

export const useMobileDetection = () => {
	const [deviceType, setDeviceType] = useState<"mobile" | "desktop">("desktop")

	useEffect(() => {
		// Client-side mobile detection using window.navigator.userAgent
		const checkMobile = () => {
			const userAgent = navigator.userAgent || (window as { opera?: string }).opera || ""

			// Simple mobile detection regex
			const mobileRegex = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i

			return mobileRegex.test(userAgent)
		}

		setDeviceType(checkMobile() ? "mobile" : "desktop")
	}, [])

	return { deviceType }
}
