"use client"

import type React from "react"
import { useId, useState } from "react"
import { Button } from "@/features/v1/frontend/shared/ui/button"
import {
	Dialog,
	DialogContent,
	DialogDescription,
	DialogFooter,
	DialogHeader,
	DialogTitle,
} from "@/features/v1/frontend/shared/ui/dialog"
import { showError } from "@/features/v1/frontend/shared/ui/global-message-store"
import { Label } from "@/features/v1/frontend/shared/ui/label"
import { Switch } from "@/features/v1/frontend/shared/ui/switch"
import { toast } from "@/features/v1/frontend/shared/ui/use-toast"
import { DeleteAccountForCurrentUser } from "@/features/v1/frontend/shared/user/user-data"

interface DeleteAccountDialogProps {
	open: boolean
	onOpenChange: (open: boolean) => void
	onDeleted?: () => void
}

export const DeleteAccountDialog: React.FC<DeleteAccountDialogProps> = ({
	open,
	onOpenChange,
	onDeleted,
}) => {
	const [deleteSavedDataToo, setDeleteSavedDataToo] = useState(false)
	const [isSubmitting, setIsSubmitting] = useState(false)
	const deleteSavedDataId = useId()

	const handleDelete = async () => {
		try {
			setIsSubmitting(true)
			await DeleteAccountForCurrentUser({ deleteFeedback: deleteSavedDataToo })
			toast({ title: "Account deleted", description: "Your account has been deleted." })
			onOpenChange(false)
			onDeleted?.()
		} catch (e) {
			showError(e instanceof Error ? e.message : "Error deleting account.")
		} finally {
			setIsSubmitting(false)
		}
	}

	return (
		<Dialog open={open} onOpenChange={onOpenChange}>
			<DialogContent>
				<DialogHeader>
					<DialogTitle>Delete your account</DialogTitle>
					<DialogDescription>
						This action cannot be undone. This will permanently delete your account. You may also
						choose to delete saved data (feedback and usage).
					</DialogDescription>
				</DialogHeader>

				<div className="flex items-center justify-between gap-x-2 mt-2">
					<div className="flex flex-col gap-y-0.5">
						<Label htmlFor={deleteSavedDataId}>Delete saved data too</Label>
						<p className="text-sm text-muted-foreground">
							Also remove feedback and usage history. This data will greatly help us improve the
							service. At no time will this data be used for training or any other purpose.
						</p>
					</div>
					<Switch
						id={deleteSavedDataId}
						checked={deleteSavedDataToo}
						onCheckedChange={setDeleteSavedDataToo}
						disabled={isSubmitting}
					/>
				</div>

				<DialogFooter className="mt-4">
					<Button variant="outline" onClick={() => onOpenChange(false)} disabled={isSubmitting}>
						Cancel
					</Button>
					<Button
						variant="destructive"
						className="ml-2 bg-primary"
						onClick={handleDelete}
						disabled={isSubmitting}
					>
						Delete account
					</Button>
				</DialogFooter>
			</DialogContent>
		</Dialog>
	)
}

export default DeleteAccountDialog
