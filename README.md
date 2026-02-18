# UvA AI V2

An improved, reverse-engineered version of UvA's AI Chat platform ([aichat.uva.nl](https://aichat.uva.nl)). Unlocks the full potential of the university's AI infrastructure by providing an OpenAI-compatible API, a better chat interface, cloud coding capabilities, automated assignment grading, and more -- all powered by models UvA already pays for but restricts behind a basic web UI.

## Why this exists

UvA gives students and staff access to GPT-5, GPT-4.1, and other models, but locks them behind a minimal chat interface with no API access, no tool integration, no coding support, and no way to use them with external tools. This project changes that.

By reverse-engineering the platform's internal API (via publicly exposed source maps), we built a full proxy layer that speaks the OpenAI API standard. This means every tool in the OpenAI ecosystem -- Claude Code, Codex CLI, Cursor, Continue, GitHub Copilot, and any OpenAI SDK client -- works with UvA's models out of the box.

## Features

### OpenAI-Compatible API

A drop-in replacement for the OpenAI API. Point any tool at your proxy URL and it just works.

- **Chat Completions** (`/api/v1/chat/completions`) -- Streaming SSE responses in standard OpenAI format. Supports system prompts, multi-turn conversations, temperature control, and token limits.
- **Responses API** (`/api/v1/responses`) -- Full implementation of OpenAI's Responses API with tool-call support and multi-turn state persistence. Enables agentic workflows where the model can call functions, receive results, and continue reasoning.
- **Model listing** (`/api/v1/models`) -- Standard model enumeration endpoint.
- **7 models available**, including ones hidden from the default UvA chat UI:
  - GPT-5, GPT-5.1, GPT-5 Mini, GPT-5 Nano
  - GPT-4.1, GPT-4o
  - GPT OSS 120B (open-source 120B parameter model)

### Cloud Coding

Use UvA's models as your coding backbone -- locally or in the cloud.

- **Claude Code compatible** -- The Responses API implements the exact protocol Claude Code expects, including streaming tool calls and `previous_response_id` chaining. Run `claude --model gpt-5` and get full agentic coding with file edits, terminal commands, and multi-step reasoning, all powered by UvA's GPT-5.
- **Codex CLI compatible** -- Works as a drop-in backend for OpenAI's Codex CLI. Set `OPENAI_BASE_URL` to your proxy and code away.
- **Cursor / Continue / Copilot** -- Any IDE extension that supports custom OpenAI endpoints works with a single config change.
- **Tool use / function calling** -- The proxy translates OpenAI tool definitions into XML system prompts that the model understands, then parses `<tool_call>` blocks from model output back into structured function call objects. This is what makes agentic coding possible -- the model can read files, run commands, and edit code through tool calls.

### Improved Chat Interface

A clean, modern dashboard that improves on UvA's default chat experience.

- **Dashboard overview** -- At-a-glance stats showing your API keys, active keys, and available models.
- **Per-key configuration** -- Create multiple API keys, each with its own default model, temperature, max tokens, system prompt, and reasoning effort. Use different configs for different tasks (one key for coding with GPT-5, another for quick questions with GPT-5 Nano).
- **Usage analytics** -- Per-key metrics: total requests, success rate, average latency, output character volume, and a 14-day daily breakdown table. Know exactly how you're using the platform.
- **Model access** -- Browse and select from all available models, including ones UvA doesn't surface in their chat UI.

### Automated Assignment Grading

Built-in grading workflow for batch-processing student submissions with AI.

- **Session management** -- Create, track, and manage grading sessions from the dashboard. Each session tracks submission count, results, and timestamps.
- **Results storage** -- Grading results are stored as structured JSON in Supabase, making them easy to export, query, and analyze.
- **Submission tracking** -- Track the number of submissions processed per session with full audit history.

### API Key Management

Self-service key management with security built in.

- **SHA-256 hashed storage** -- Raw keys are never stored. The key is shown exactly once at creation time, then only the hash is kept.
- **Per-key defaults** -- Each key carries its own model, temperature, max tokens, system prompt, and reasoning effort. These are used when the API request doesn't specify overrides.
- **Enable / disable** -- Toggle keys on and off without deleting them. Useful for rotating keys or temporarily revoking access.
- **Usage audit** -- Every request is logged with the API key ID, model used, input/output character counts, latency, and success status.

### Chrome Extension

A Manifest V3 Chrome extension that eliminates the most annoying part of the setup -- session cookie management.

- **Automatic sync** -- Watches for cookie changes on `aichat.uva.nl` and automatically pushes them to the proxy. Log into UvA once and the proxy stays authenticated.
- **Debounced updates** -- Batches multiple cookie changes (which happen during login) into a single sync with a 2-second debounce.
- **Status feedback** -- Badge icon shows green (synced), red (error), or yellow (not configured). The popup shows the last sync result and timestamp.
- **Manual sync** -- One-click "Sync Now" button for when you need to force a refresh.

## How it was built

The entire project was built by reverse-engineering UvA's AI Chat frontend:

1. **Source map extraction** -- UvA's Next.js deployment ships public source maps at `/_next/static/chunks/{hash}.js.map`. The original TypeScript source was recovered from these maps, revealing the full API surface, server action IDs, request formats, and authentication flow.

2. **Request format discovery** -- The upstream platform uses Vercel AI SDK v2 with a custom SSE format (`{"type":"text-delta","delta":"..."}`). Each request requires fresh UUID v4 identifiers for both the thread and message. The proxy generates these automatically.

3. **Authentication chain** -- Users authenticate via Azure AD (UvA's SSO). The session cookies from `aichat.uva.nl` (`__Host-authjs.csrf-token` and `authjs.session-token`) are captured and stored per-user, then injected into every upstream request.

4. **Translation layer** -- The proxy translates bidirectionally between OpenAI and UvA formats:
   - OpenAI `messages` array becomes UvA's `{id, message, flags, overrides}` structure
   - UvA's `text-delta` SSE events become OpenAI's `choices[0].delta.content` format
   - Tool definitions become XML system prompts; XML tool-call blocks in output become OpenAI function call objects

5. **State persistence** -- Multi-turn Responses API conversations are persisted in Supabase with a 2-hour TTL, supporting `previous_response_id` chaining that matches OpenAI's stateful conversation model.

## Architecture

```
User / External Tool
    |
    |  Standard OpenAI API (Bearer key auth)
    v
+---------------------------+
|  UvA AI V2 Proxy          |
|  (Next.js 15 on Vercel)   |
|                            |
|  - Translates formats      |
|  - Manages API keys        |
|  - Tracks usage            |
|  - Handles tool calls      |
|  - Persists state          |
+---------------------------+
    |
    |  UvA internal format + session cookies
    v
+---------------------------+
|  aichat.uva.nl            |
|  (UvA's AI platform)      |
|                            |
|  GPT-5, GPT-4.1, etc.     |
+---------------------------+
    |
    |  Vercel AI SDK v2 SSE stream
    v
Proxy translates back -> OpenAI SSE format -> Tool
```

**Stack:** Next.js 15 (App Router, Turbopack), Bun, TypeScript, Supabase (Postgres), NextAuth.js (Azure AD), Vercel.

## Setup

### Prerequisites

- [Bun](https://bun.sh)
- A Supabase project (free tier works)
- Azure AD credentials (UvA tenant)

### Quick start

```bash
git clone https://github.com/tr4m0ryp/UVA_AI_V2.git
cd UVA_AI_V2
cp .env.example .env   # Fill in your credentials
./run                   # Installs deps, validates env, starts dev server
```

### Environment variables

| Variable | Description |
|----------|-------------|
| `NEXT_PUBLIC_SUPABASE_URL` | Supabase project URL |
| `NEXT_PUBLIC_SUPABASE_ANON_KEY` | Supabase anonymous key |
| `SUPABASE_SERVICE_ROLE_KEY` | Supabase service role key |
| `NEXTAUTH_URL` | Your app URL (e.g. `http://localhost:3000`) |
| `NEXTAUTH_SECRET` | Random 32-char secret for session encryption |
| `AZURE_AD_CLIENT_ID` | Azure AD application client ID |
| `AZURE_AD_CLIENT_SECRET` | Azure AD client secret |
| `AZURE_AD_TENANT_ID` | Azure AD tenant ID |

### Database

Run the SQL files in `supabase/migrations/` against your Supabase project (dashboard SQL editor or CLI).

### Chrome extension

1. Open `chrome://extensions` and enable Developer Mode
2. Click "Load unpacked" and select the `extension/` directory
3. Click the extension icon, enter your proxy URL and an API key
4. Log into `aichat.uva.nl` -- cookies sync automatically

## Usage examples

### Claude Code (cloud coding)

```bash
export OPENAI_BASE_URL=https://your-proxy.vercel.app/api/v1
export OPENAI_API_KEY=uva-your-api-key-here

claude --model gpt-5
```

Full agentic coding: file reads, edits, terminal commands, multi-step reasoning -- all through UvA's GPT-5.

### OpenAI Codex CLI

```bash
export OPENAI_BASE_URL=https://your-proxy.vercel.app/api/v1
export OPENAI_API_KEY=uva-your-api-key-here

codex
```

### Python (OpenAI SDK)

```python
from openai import OpenAI

client = OpenAI(
    base_url="https://your-proxy.vercel.app/api/v1",
    api_key="uva-your-api-key-here",
)

response = client.chat.completions.create(
    model="gpt-5",
    messages=[{"role": "user", "content": "Hello"}],
    stream=True,
)

for chunk in response:
    print(chunk.choices[0].delta.content or "", end="")
```

### curl

```bash
curl https://your-proxy.vercel.app/api/v1/chat/completions \
  -H "Authorization: Bearer uva-your-api-key-here" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4o",
    "messages": [{"role": "user", "content": "Hello"}],
    "stream": true
  }'
```

## API reference

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/api/v1/chat/completions` | OpenAI-compatible streaming chat completions |
| `POST` | `/api/v1/responses` | Responses API with tool-call and multi-turn support |
| `GET` | `/api/v1/models` | List all available models |
| `PUT` | `/api/v1/session` | Update UvA session cookie (bearer-key auth, used by extension) |
| `GET` | `/api/health` | Health check |

## Roadmap

Planned features for upcoming releases:

- **Built-in chat UI** -- A full chat interface directly in the dashboard, replacing UvA's limited one. Conversation history, model switching mid-chat, artifact rendering, and image generation support.
- **Advanced grading pipeline** -- Rubric parsing, file upload (PDF/DOCX), batch auto-grading with configurable models, score distribution visualization, and CSV/Excel export.
- **Rate limiting and quotas** -- Per-key rate limits and daily token quotas to prevent abuse and share access fairly.
- **Conversation history** -- Persistent chat history stored in Supabase, searchable and resumable across sessions.
- **Multi-user sharing** -- Team API keys and shared grading sessions for course staff.
- **Streaming tool calls** -- Real-time streaming of tool-call arguments as they're generated, instead of buffering the full response.
- **Image and file support** -- Vision model support (image inputs) and file attachment handling through the API.
- **Firefox extension** -- Port the Chrome extension to Firefox with WebExtension APIs.

## Disclaimer

This project is for educational and research purposes. It relies on reverse-engineering a university platform and may break if UvA changes their internal API. Use responsibly and in accordance with your institution's acceptable use policies.
