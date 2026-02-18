#!/bin/bash
# Test script for UvA AI Chat /api/v1/chat endpoint
# Requires a valid session cookie - set via environment or proxy.env
#
# Usage:
#   UVA_COOKIE="your-session-cookie-here" ./test_chat.sh
#
# Or with the proxy running:
#   ./test_chat.sh --proxy

set -e

BASE_URL="https://aichat.uva.nl"
PROXY_URL="http://127.0.0.1:8787"

# Load cookie from env or proxy.env
if [ -z "$UVA_COOKIE" ]; then
    if [ -f proxy.env ]; then
        UVA_COOKIE=$(grep '^UVA_SESSION_COOKIE' proxy.env | sed 's/^[^=]*=//; s/^"//; s/"$//')
    fi
fi

if [ -z "$UVA_COOKIE" ]; then
    echo "ERROR: No session cookie available."
    echo "Set UVA_COOKIE env var or configure proxy.env"
    echo ""
    echo "To get a cookie:"
    echo "  1. Log in to aichat.uva.nl in Firefox"
    echo "  2. Open DevTools (F12) -> Console"
    echo "  3. Run: document.cookie"
    echo "  4. Copy the output and set: export UVA_COOKIE='...'"
    exit 1
fi

echo "=== Test 1: Validate session ==="
SESSION=$(curl -s --max-time 10 \
    -H "Cookie: $UVA_COOKIE" \
    "$BASE_URL/api/auth/session")
echo "$SESSION" | python3 -m json.tool 2>/dev/null || echo "$SESSION"
echo ""

if echo "$SESSION" | grep -q '"user"'; then
    echo "Session is VALID"
else
    echo "Session is INVALID or expired"
    exit 1
fi
echo ""

echo "=== Test 2: Fetch available models (server action) ==="
MODELS=$(curl -s --max-time 10 \
    -X POST "$BASE_URL/" \
    -H "Cookie: $UVA_COOKIE" \
    -H "Content-Type: text/plain;charset=UTF-8" \
    -H "Next-Action: 7f97bd7faa76e73dd6f8166e9eb66eda8d773789aa" \
    -d '[]')
echo "Response (first 500 chars): ${MODELS:0:500}"
echo ""

echo "=== Test 3: Create chat thread (server action) ==="
THREAD_ID=$(python3 -c "import random,string; print(''.join(random.choices(string.ascii_letters+string.digits, k=36)))")
echo "Thread ID: $THREAD_ID"
CREATE_RESP=$(curl -s --max-time 10 \
    -X POST "$BASE_URL/" \
    -H "Cookie: $UVA_COOKIE" \
    -H "Content-Type: text/plain;charset=UTF-8" \
    -H "Next-Action: 7f5606233197c25071eb2ef91303c87f6ccc18fa87" \
    -d "[{\"id\":\"$THREAD_ID\",\"name\":\"Proxy test\",\"personaMessage\":\"\",\"personaMessageTitle\":\"\",\"isBookmarked\":false,\"type\":\"chat\"}]")
echo "Response (first 500 chars): ${CREATE_RESP:0:500}"
echo ""

echo "=== Test 4: Chat API - minimal request ==="
RESP1=$(curl -s --max-time 30 -D /tmp/uva_chat_headers.txt \
    -X POST "$BASE_URL/api/v1/chat" \
    -H "Cookie: $UVA_COOKIE" \
    -H "Content-Type: application/json" \
    -d "{\"id\":\"$THREAD_ID\"}")
echo "Headers:"
cat /tmp/uva_chat_headers.txt
echo "Body (first 1000 chars): ${RESP1:0:1000}"
echo ""

echo "=== Test 5: Chat API - with messages and model ==="
RESP2=$(curl -s --max-time 60 -D /tmp/uva_chat_headers2.txt \
    -X POST "$BASE_URL/api/v1/chat" \
    -H "Cookie: $UVA_COOKIE" \
    -H "Content-Type: application/json" \
    -d "{
        \"id\":\"$THREAD_ID\",
        \"messages\":[{\"role\":\"user\",\"content\":\"Say hello in exactly 5 words.\"}],
        \"model\":\"gpt-4.1-mini\",
        \"temperature\":0.5,
        \"top_p\":0.5
    }")
echo "Headers:"
cat /tmp/uva_chat_headers2.txt
echo "Body (first 2000 chars): ${RESP2:0:2000}"
echo ""

echo "=== Test 6: Chat API - streaming with Accept header ==="
curl -s --max-time 60 -N \
    -X POST "$BASE_URL/api/v1/chat" \
    -H "Cookie: $UVA_COOKIE" \
    -H "Content-Type: application/json" \
    -H "Accept: text/event-stream" \
    -d "{
        \"id\":\"$THREAD_ID\",
        \"messages\":[{\"role\":\"user\",\"content\":\"Count from 1 to 5.\"}],
        \"model\":\"gpt-4.1-mini\",
        \"temperature\":0.5,
        \"top_p\":0.5
    }" 2>&1 | head -50
echo ""

echo "=== Test 7: Cleanup - delete thread ==="
DEL_RESP=$(curl -s --max-time 10 \
    -X POST "$BASE_URL/" \
    -H "Cookie: $UVA_COOKIE" \
    -H "Content-Type: text/plain;charset=UTF-8" \
    -H "Next-Action: 7ff009fa85af417d5914c562140adf3847b804201b" \
    -d "[\"$THREAD_ID\"]")
echo "Delete response: ${DEL_RESP:0:200}"
echo ""

echo "=== Done ==="
echo "Save complete output to analyze the exact request/response format."
