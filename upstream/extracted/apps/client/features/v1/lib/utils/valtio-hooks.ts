/**
 * Custom Valtio hooks that remove the DeepReadonly constraint from useSnapshot.
 *
 * After upgrading to Next.js 15 + React 19, TypeScript's stricter type checking
 * started enforcing valtio's DeepReadonly types more rigorously, causing widespread
 * type errors when passing snapshot values to functions expecting mutable types.
 *
 * This wrapper removes the readonly constraint at the source, eliminating the need
 * to add ReadonlySupport types throughout the entire codebase.
 *
 * See: https://github.com/pmndrs/valtio/discussions/451
 */

import { useSnapshot as useSnapshotOriginal } from "valtio"

/**
 * Utility type to remove readonly modifiers deeply from an object
 */
export type DeepWritable<T> = T extends (...args: any[]) => any
	? T
	: T extends object
		? { -readonly [P in keyof T]: DeepWritable<T[P]> }
		: T

/**
 * Custom useSnapshot hook that returns a writable version of the snapshot.
 *
 * Note: The snapshot is still immutable at runtime (valtio prevents mutations).
 * This only removes TypeScript's readonly constraints to allow passing snapshot
 * values to functions expecting mutable types without type errors.
 *
 * @param proxyObject - The valtio proxy state object
 * @param options - Optional valtio snapshot options (e.g., { sync: true })
 * @returns A snapshot of the state without readonly constraints
 */
export function useSnapshot<T extends object>(
	proxyObject: T,
	options?: { sync?: boolean },
): DeepWritable<T> {
	const snap = useSnapshotOriginal(proxyObject, options as any)
	return snap as DeepWritable<T>
}
