# Codex + Proxy Test Results -- 2026-02-22

Test environment: Codex CLI v0.104.0, proxy at localhost:8787, all 7 models.

---

## Test 1: Responses API -- Non-Streaming Text (no tools)

Simple prompt: "Reply with exactly: TEST_OK_<MODEL>"

| Model        | Status | Notes                     |
|--------------|--------|---------------------------|
| gpt-4o       | PASS   | Clean response            |
| gpt-4.1      | PASS   | Clean response            |
| gpt-5-nano   | PASS   | Clean response            |
| gpt-5-mini   | PASS   | Clean response            |
| gpt-5        | PASS   | Clean response            |
| gpt-5.1      | PASS   | Clean response            |
| gpt-oss-120b | PASS   | Clean response            |

---

## Test 2: Responses API -- Streaming Text (no tools)

Prompt: "Say TEST_STREAM_OK"

| Model        | Status | Notes                                          |
|--------------|--------|-------------------------------------------------|
| gpt-4o       | PASS   | Clean SSE events, correct sequence_number       |
| gpt-4.1      | PASS   | Clean SSE events                                |
| gpt-5-nano   | PASS   | Clean SSE events                                |
| gpt-5-mini   | WARN   | Wraps response in markdown code blocks           |
| gpt-5        | PASS   | Clean SSE events                                |
| gpt-5.1      | PASS   | Clean SSE events                                |
| gpt-oss-120b | PASS   | Clean SSE events                                |

---

## Test 3: Responses API -- Tool Calling (Codex exec_command format)

Request includes Codex-style flat tool definition for exec_command.
Prompt: "List files in the current directory"

| Model        | Status | Tool Name Correct | Arg Key     | Arg Type  | Notes                               |
|--------------|--------|-------------------|-------------|-----------|---------------------------------------|
| gpt-4o       | WARN   | YES               | "cmd"       | string    | Wrong key (should be "command"), wrong type (should be array) |
| gpt-4.1      | PASS   | YES               | "command"   | array     | Correct format: {"command":["ls","-l"]}  |
| gpt-5-nano   | PASS   | YES               | "command"   | array     | Correct format: {"command":["ls","-la"]} |
| gpt-5-mini   | WARN   | YES               | "cmd"       | string    | Wrong key and type                    |
| gpt-5        | WARN   | YES               | "cmd"       | string    | Wrong key and type                    |
| gpt-5.1      | WARN   | YES               | "cmd"       | string    | Wrong key and type                    |
| gpt-oss-120b | WARN   | YES               | "cmd"       | string    | Wrong key and type                    |

Root cause: The proxy's tool XML prompt uses `"cmd"` in its examples/template
(see RETRY_FORCE_MSG line 147: `"cmd": "COMMAND"`), and the code block extraction
fallback synthesizes calls with `{"cmd": "..."}`. Models that go through the
codeblock extraction path inherit this wrong key name. gpt-4.1 and gpt-5-nano
produce native XML with the correct schema-defined parameter names.

---

## Test 4: Codex CLI -- Simple Prompt (per model)

Prompt: "What is 2+2? Reply with just the number."
Timeout: 30-60s, non-interactive (codex exec)

| Model        | Status | Behavior                                                |
|--------------|--------|---------------------------------------------------------|
| gpt-4o       | FAIL   | Infinite loop: keeps calling `echo $((2+2))` forever    |
| gpt-4.1      | FAIL   | Infinite loop: keeps calling `echo 4` forever            |
| gpt-5-nano   | FAIL   | Infinite loop: keeps calling `printf 4` forever          |
| gpt-5-mini   | PASS   | Responds with "4" as text, terminates correctly          |
| gpt-5        | FAIL   | Infinite loop: keeps calling `printf '4'` forever        |
| gpt-5.1      | PASS   | One tool call then text response, terminates correctly   |
| gpt-oss-120b | FAIL   | Infinite loop: keeps calling `echo 4` forever            |

---

## Test 5: Codex CLI -- Multi-Step Task

Prompt: "Create a file, then read it back and confirm the contents."

| Model        | Status | Behavior                                          |
|--------------|--------|---------------------------------------------------|
| gpt-5-mini   | FAIL   | Infinite loop: keeps running cat/heredoc commands  |
| gpt-5.1      | FAIL   | Infinite loop: keeps running same write+read       |

Only gpt-5-mini and gpt-5.1 passed the simple test; both fail multi-step.

---

## Test 6: chat/completions -- Non-Streaming

All 7 models PASS. Response format is correct OpenAI format.

---

## Test 7: chat/completions -- Streaming

All 7 models PASS. SSE format correct, proper `finish_reason: "stop"`.

---

## BUG LIST

### BUG-1: CRITICAL -- Infinite Tool Call Loop in Codex

**Severity:** Critical (makes Codex unusable for all models)

**Symptom:** Codex enters an infinite loop where the same tool call is
executed repeatedly. The model never produces a plain text response to
end the conversation. Affects ALL 7 models on multi-step tasks, and
5/7 on simple tasks.

**Root cause (proxy-side):** The `handle_responses()` function in
`route.c:132` uses `if (rr.has_tools)` to decide the execution path.
When tools are present (which they always are with Codex), ALL model
responses go through `resp_tool_request_with_retry()`, which:

1. Sends the request to upstream
2. If model returns plain text (no XML tool calls): retries with forcing prompt
3. If retry also fails: extracts any code block or inline backtick as synthetic tool call
4. Returns a function_call to Codex

This means the proxy NEVER returns a text response when tools are defined.
Even when the model explicitly wants to answer with text (e.g., "4"), the
proxy forces it into a tool call.

**Additional factor:** The `tool_choice` parameter is logged but not honored.
When Codex sends `tool_choice: "auto"`, the proxy ignores it and always
forces tool calls.

**Required fix:**
- When model responds with text and `tool_choice` is "auto" or "none":
  check if the text looks like a conversational response vs. a failed tool
  call attempt. If conversational, return it as a text message, not a tool call.
- Honor `tool_choice: "none"` to completely skip tool extraction.
- Implement a turn limit or detect repeated identical tool calls to break loops.

---

### BUG-2: HIGH -- Tool Call Arguments Use Wrong Key Name

**Severity:** High

**Symptom:** 5 of 7 models return `{"cmd": "ls -la"}` instead of the
schema-defined `{"command": ["ls", "-la"]}`.

**Root cause:** The proxy's retry forcing template (`RETRY_FORCE_MSG`) and
code block extraction fallback (`resp_synthesize_tool_call()`) use `"cmd"`
as the argument key. Models that go through the codeblock extraction path
inherit this wrong key.

**Affected models:** gpt-4o, gpt-5-mini, gpt-5, gpt-5.1, gpt-oss-120b
(all "resistant" models that use codeblock extraction)

**Working models:** gpt-4.1, gpt-5-nano (native XML, correct schema keys)

**Required fix:**
- `resp_synthesize_tool_call()` should use `"command"` key and array format
  matching the tool schema, not hardcoded `"cmd"` string.
- Alternatively, parse the actual tool schema and use the defined parameter
  names when synthesizing tool calls.

---

### BUG-3: MEDIUM -- No Conversation Context Between Turns

**Severity:** Medium

**Symptom:** Every Codex request shows `prev=` (empty) in proxy logs. The
proxy treats each turn as independent, even though Codex sends full
conversation history in the `input` array.

**Root cause:** Codex does NOT use `previous_response_id` for chaining.
Instead, it sends the full conversation (including function_call and
function_call_output items) in each request's `input` array. The proxy
DOES process these (via `resp_input_to_messages()` and
`resp_fold_tool_results()`), but the resulting folded message may lose
context when sent through the UvA single-message translator.

**Impact:** When the model receives a request with tool results folded in,
it may not properly understand the conversation flow, leading to repeated
tool calls instead of progressing.

**Required fix:**
- Verify that `resp_fold_tool_results()` produces output that clearly
  conveys: "you already called this tool and got this result, now respond"
- Consider adding explicit instructions like "Based on the tool results
  above, provide your final answer as text."

---

### BUG-4: MEDIUM -- gpt-5-mini Wraps Responses in Code Blocks

**Severity:** Medium

**Symptom:** gpt-5-mini wraps even simple text responses in markdown code
blocks: ` ```\nTEST_STREAM_OK\n``` `

**Root cause:** The "resistant model" prompt used for gpt-5-mini frames
the model as a "command writer" and asks for code blocks. This prompt
bleeds into non-tool responses.

**Impact:** Text responses from gpt-5-mini contain unnecessary markdown
formatting. May confuse Codex's response parsing.

---

### BUG-5: LOW -- Model Metadata Not Found Warning

**Severity:** Low

**Symptom:** Codex warns: "Model metadata for `gpt-4o` not found.
Defaulting to fallback metadata; this can degrade performance."
Appears for gpt-4o, gpt-4.1, gpt-oss-120b (not for gpt-5 family).

**Root cause:** Codex has built-in metadata for gpt-5 family models
but not for gpt-4o/4.1/oss-120b. The proxy does not serve
`/v1/models/{model_id}` endpoint (returns 404).

**Required fix:**
- Consider adding model metadata endpoints, OR
- Codex config can potentially include model metadata overrides.
- Low priority since it doesn't break functionality.

---

### BUG-6: LOW -- Usage/Token Counts Always Null/Zero

**Severity:** Low

**Symptom:** All API responses return `"usage": null` or `"usage":
{"input_tokens":0,"output_tokens":0,"total_tokens":0}`. Codex shows
"tokens used: 0" at end of session.

**Root cause:** The UvA upstream API does not return token counts, and
the proxy doesn't estimate them.

**Impact:** No token tracking or cost estimation possible. Cosmetic issue
for Codex, but could matter for usage monitoring.

---

### BUG-7: LOW -- tool_choice Parameter Ignored

**Severity:** Low (but contributes to BUG-1)

**Symptom:** `tool_choice` is parsed from the request and logged but
never used in routing logic.

**Root cause:** `resp_parse_request()` logs the value but doesn't store it.
`route.c` only checks `has_tools` to decide the execution path.

**Required fix:**
- Store `tool_choice` in `resp_request_t`
- When `tool_choice` is "none": skip tool injection and extraction entirely
- When `tool_choice` is "auto": allow text responses to pass through
  without forced tool extraction

---

## PRIORITY ORDER FOR FIXES

1. **BUG-1** (Critical): Fix infinite loop -- this blocks all Codex usage
2. **BUG-7** (Low but prerequisite): Implement tool_choice handling (needed for BUG-1 fix)
3. **BUG-2** (High): Fix synthesized tool call argument format
4. **BUG-3** (Medium): Improve conversation context handling
5. **BUG-4** (Medium): Fix gpt-5-mini code block wrapping
6. **BUG-5** (Low): Model metadata endpoints
7. **BUG-6** (Low): Token usage tracking

---

## SUMMARY

The proxy's Responses API works correctly for basic text generation (both
streaming and non-streaming) on all 7 models. The chat/completions API
also works perfectly.

The critical blocker is the infinite tool-call loop (BUG-1), which makes
Codex completely unusable for real tasks on all models. The root cause is
that the proxy's tool handling path always forces a tool call response,
even when the model wants to reply with text. Combined with Codex not
using `previous_response_id` (instead sending full history in `input`),
the model keeps seeing its tool call without understanding it already
executed, leading to infinite repetition.

The fix requires the proxy to distinguish between "model wants to use a
tool" and "model wants to respond with text" when `tool_choice` is "auto".
