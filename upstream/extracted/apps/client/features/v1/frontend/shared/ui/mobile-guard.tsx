"use client"

import Image from "next/image"
import { useEffect, useState } from "react"
import { getLogoName, getSystemName, getTextName } from "../theme/theme-config"
import { useMobileDetection } from "./hooks/use-mobile-detection"

interface MobileGuardProps {
	children: React.ReactNode
}

// GAI-258: Converted from React.FC to function component syntax for React 19 compatibility
export const MobileGuard = ({ children }: MobileGuardProps) => {
	const { deviceType } = useMobileDetection()
	const [hasMounted, setHasMounted] = useState(false)

	useEffect(() => {
		setHasMounted(true)
	}, [])

	// Prevent hydration mismatch by not rendering mobile-specific content on server
	if (!hasMounted) {
		return <>{children}</>
	}

	if (deviceType === "mobile") {
		return (
			<div className="flex items-center justify-center min-h-screen bg-gray-50">
				<div className="text-left p-8 max-w-md mx-auto">
					<div className="flex items-center text-4xl font-serif gap-4 mb-12">
						<Image
							src={`/${getLogoName()}`}
							alt={`${getTextName()} logo`}
							width={96}
							height={96}
							className="dark:invert"
						/>

						<span>{getSystemName()}</span>
					</div>

					<h1 className="text-3xl font-bold text-gray-900 mb-6">Mobile Not Supported Yet</h1>
					<div className="flex flex-col gap-4 text-gray-800 text-lg">
						<p>
							We understand that you&apos;d like to use {getSystemName()} on your mobile, but
							we&apos;re not quite there yet. {getSystemName()} currently only works on a laptop or
							desktop.
						</p>
						<p className="font-semibold">
							Grab a bigger screen and you can get started right away.
						</p>
						<p>Of course, a mobile version is on our roadmap.</p>
					</div>
				</div>
			</div>
		)
	}

	return <>{children}</>
}
