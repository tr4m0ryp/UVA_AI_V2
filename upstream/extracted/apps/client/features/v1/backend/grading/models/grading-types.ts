export type GradingSessionStatus =
	| "draft"
	| "parsing_rubric"
	| "calibrating"
	| "grading"
	| "normalizing"
	| "completed"
	| "failed"

export type SubmissionStatus =
	| "pending"
	| "grading"
	| "completed"
	| "re_grading"
	| "failed"

export type AnchorLevel = "HIGH" | "MID" | "LOW"

export interface RubricLevel {
	range: [number, number]
	label: string
	description: string
}

export interface RubricCriterion {
	name: string
	max_points: number
	levels: RubricLevel[]
}

export interface StructuredRubric {
	criteria: RubricCriterion[]
	total_max: number
	pass_threshold: number
}

export interface CriterionScore {
	criterion_name: string
	score: number
	max_points: number
}

export interface AnchorSubmission {
	student_identifier: string
	content_excerpt: string
	scores: CriterionScore[]
	total_score: number
}

export interface AnchorData {
	high: AnchorSubmission
	mid: AnchorSubmission
	low: AnchorSubmission
}

export interface SessionStats {
	mean: number
	median: number
	std_dev: number
	min: number
	max: number
	pass_rate: number
	total_graded: number
	total_failed: number
	anomalies_regraded: number
}

export interface GradingSession {
	id: string
	userId: string
	name: string
	assignmentText: string
	assignmentFilePath: string | null
	rubricRaw: string
	rubricStructured: StructuredRubric | null
	modelAnswer: string
	modelAnswerFilePath: string | null
	selectedModel: string | null
	modelReasoning: string | null
	status: GradingSessionStatus
	anchorData: AnchorData | null
	stats: SessionStats | null
	totalSubmissions: number
	gradedCount: number
	createdAt: Date
	updatedAt: Date
}

export interface Submission {
	id: string
	sessionId: string
	studentIdentifier: string
	filePath: string
	contentExtracted: string | null
	status: SubmissionStatus
	isAnchor: boolean
	anchorLevel: AnchorLevel | null
	scores: CriterionScore[] | null
	totalScore: number | null
	feedback: string | null
	improvementComments: string | null
	reasoning: string | null
	gradedAt: Date | null
	createdAt: Date
}

export interface LLMGradingResponse {
	scores: CriterionScore[]
	total_score: number
	feedback: string
	improvement_comments: string
	reasoning: string
}
