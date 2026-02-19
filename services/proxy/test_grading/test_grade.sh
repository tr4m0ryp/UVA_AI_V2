#!/bin/bash
# End-to-end test: grade 3 submissions (excellent, average, bad) via the proxy
# Mimics exactly what dashboard/js/grading-api.js does
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
BASE="http://localhost:8787"
KEY="uva-sk-7d0873b04d8f907bbc5867368f87b96d8e3c9dfac87ed16db91559c1923c1798"
MODEL="gpt-5-nano"

ASSIGNMENT=$(cat "$DIR/assignment.txt")
RUBRIC=$(cat "$DIR/rubric.txt")
EXAMPLE=$(cat "$DIR/example_answer.txt")

# Build system prompt -- same as grading-api.js buildSystemPrompt()
SYSTEM_PROMPT="You are a strict academic grader. Grade the following student submission based on the provided rubric.

ASSIGNMENT:
$ASSIGNMENT

RUBRIC:
$RUBRIC

EXAMPLE ANSWER (for reference):
$EXAMPLE

You MUST respond with ONLY a valid JSON object (no markdown, no text outside JSON) in this exact format:
{
  \"criteria\": [
    { \"name\": \"<criterion name>\", \"score\": <number>, \"maxScore\": <number>, \"feedback\": \"<specific feedback>\" }
  ],
  \"overallFeedback\": \"<2-3 sentence summary>\"
}

Rules:
- Extract criteria names and max scores from the rubric
- Score each criterion from 0 to its maxScore
- Be rigorous and fair
- Provide specific, actionable feedback"

# Test files: one excellent, one average, one bad
TEST_FILES=(
  "emma_de_vries_14000000.py|Emma de Vries|14000000"
  "ethan_peters_14000370.py|Ethan Peters|14000370"
  "henry_prins_14001120.py|Henry Prins|14001120"
)

for entry in "${TEST_FILES[@]}"; do
  IFS='|' read -r fname name sid <<< "$entry"
  CONTENT=$(cat "$DIR/submissions/$fname")

  echo "=== Grading: $name ($sid) -- $fname ==="

  USER_MSG="Student: $name (ID: $sid)

--- BEGIN SUBMISSION ---
$CONTENT
--- END SUBMISSION ---"

  # Build JSON payload (use python for proper escaping)
  PAYLOAD=$(python3 -c "
import json, sys
print(json.dumps({
    'model': '$MODEL',
    'messages': [
        {'role': 'system', 'content': sys.stdin.read()},
        {'role': 'user', 'content': '''$USER_MSG'''}
    ],
    'temperature': 0.2,
    'stream': False
}))
" <<< "$SYSTEM_PROMPT")

  # Send request
  RESP=$(curl -s --max-time 60 "$BASE/v1/chat/completions" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $KEY" \
    -d "$PAYLOAD")

  # Extract the content from the response
  echo "$RESP" | python3 -c "
import json, sys
try:
    r = json.load(sys.stdin)
    if 'error' in r:
        print('ERROR:', r['error'])
        sys.exit(1)
    content = r['choices'][0]['message']['content']
    # Strip markdown fences if present
    if content.startswith('\`\`\`'):
        lines = content.split('\n')
        content = '\n'.join(lines[1:-1])
    parsed = json.loads(content)
    total = sum(c['score'] for c in parsed['criteria'])
    maxTotal = sum(c['maxScore'] for c in parsed['criteria'])
    pct = round(total / maxTotal * 100) if maxTotal > 0 else 0
    print(f'Score: {total}/{maxTotal} ({pct}%)')
    for c in parsed['criteria']:
        print(f'  {c[\"name\"]}: {c[\"score\"]}/{c[\"maxScore\"]}')
    print(f'Feedback: {parsed[\"overallFeedback\"][:120]}...')
except Exception as e:
    print(f'Parse error: {e}')
    print('Raw:', repr(r.get('choices', [{}])[0].get('message', {}).get('content', '')[:200]))
"
  echo ""
done
