#!/bin/bash
set -e

# Required env vars:
#   REPO_FULL_NAME   - "owner/repo"
#   GITHUB_TOKEN     - GitHub access token
#   BRANCH_NAME      - branch to create (e.g. "codex/task-7")
#   MODEL            - model name for codex
#   INSTRUCTIONS     - task instructions
#   OPENAI_API_KEY   - proxy API key
#   PROXY_BASE_URL   - proxy URL (e.g. "http://host.docker.internal:8787")

echo "[entrypoint] Cloning ${REPO_FULL_NAME}..."
git clone "https://${GITHUB_TOKEN}@github.com/${REPO_FULL_NAME}.git" /workspace
cd /workspace

echo "[entrypoint] Creating branch ${BRANCH_NAME}..."
git checkout -b "${BRANCH_NAME}"

git config user.name "UvA Cloud Coding"
git config user.email "cloud-coding@uva-proxy.local"

# Point codex at our proxy
export OPENAI_BASE_URL="${PROXY_BASE_URL}/v1"

echo "[entrypoint] Starting codex with model ${MODEL}..."
echo "[entrypoint] Instructions: ${INSTRUCTIONS}"

# Run codex in full-auto mode
codex --model "${MODEL}" --full-auto "${INSTRUCTIONS}" || true

# After codex exits, commit any remaining changes and push
echo "[entrypoint] Codex finished. Pushing final changes..."
git add -A
if ! git diff --cached --quiet; then
    git commit -m "Cloud Coding: final changes"
fi

git push origin "${BRANCH_NAME}" || {
    echo "[entrypoint] Push failed -- setting up remote tracking"
    git push --set-upstream origin "${BRANCH_NAME}"
}

echo "[entrypoint] Done. Branch ${BRANCH_NAME} pushed."
