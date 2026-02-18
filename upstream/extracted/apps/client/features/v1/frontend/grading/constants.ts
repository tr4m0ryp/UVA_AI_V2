export const STATUS_LABELS: Record<string, string> = {
	draft: "Draft",
	parsing_rubric: "Parsing Rubric",
	calibrating: "Calibrating",
	grading: "Grading",
	normalizing: "Normalizing",
	completed: "Completed",
	failed: "Failed",
}

export const STATUS_COLORS: Record<string, string> = {
	draft: "bg-muted text-muted-foreground",
	parsing_rubric: "bg-blue-100 text-blue-800 dark:bg-blue-900 dark:text-blue-200",
	calibrating: "bg-blue-100 text-blue-800 dark:bg-blue-900 dark:text-blue-200",
	grading: "bg-yellow-100 text-yellow-800 dark:bg-yellow-900 dark:text-yellow-200",
	normalizing: "bg-yellow-100 text-yellow-800 dark:bg-yellow-900 dark:text-yellow-200",
	completed: "bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-200",
	failed: "bg-red-100 text-red-800 dark:bg-red-900 dark:text-red-200",
}

export const MODEL_LABELS: Record<string, string> = {
	"claude-sonnet-4.5": "Claude Sonnet 4.5 (humanities, essays)",
	o3: "o3 (STEM, math, logic)",
	"gpt-5.1": "GPT-5.1 (code, technical reports)",
}

export const ACCEPTED_FILE_EXTENSIONS = ["pdf", "txt"] as const

export const ACTIVE_STATUSES = [
	"calibrating",
	"grading",
	"normalizing",
	"parsing_rubric",
] as const
