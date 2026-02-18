# UvA AI Proxy

An improved, reverse-engineered version of UvA's AI Chat platform ([aichat.uva.nl](https://aichat.uva.nl)) that turns it into a fully OpenAI-compatible API. Create personal API keys and use UvA's GPT models with any external tool -- Claude Code, OpenAI Codex CLI, GitHub Copilot, Continue, Cursor, or any client that speaks the OpenAI API format.

## What this does

UvA provides students and staff with access to GPT models through a web chat interface. This project reverse-engineers that platform and wraps it in a standard OpenAI-compatible API, giving you:

- **API key management** -- Generate personal API keys from a dashboard, each with its own default model, temperature, system prompt, and token limits. Keys are hashed with SHA-256 and stored securely; the raw key is shown only once.

- **OpenAI-compatible endpoints** -- Drop-in replacement for the OpenAI API. Point any tool at your proxy URL and it works out of the box:
  - `POST /api/v1/chat/completions` -- Streaming chat completions (SSE)
  - `POST /api/v1/responses` -- Responses API with full tool-call support
  - `GET /api/v1/models` -- List available models

- **More models than the UvA chat UI exposes** -- Access all models available on the platform, including ones not shown in the default chat interface:
  - GPT-5, GPT-5.1, GPT-5 Mini, GPT-5 Nano
  - GPT-4.1, GPT-4o
  - GPT OSS 120B

- **Tool use / function calling** -- The Responses API (`/api/v1/responses`) supports OpenAI-style tool definitions and multi-turn tool-call flows. Tools are injected as XML system prompts, and the proxy parses `<tool_call>` blocks from model output back into structured function call objects. This enables agentic workflows with Claude Code, Codex, and similar tools.

- **Per-key configuration** -- Each API key can be customized with:
  - Default model
  - Temperature, max tokens
  - System prompt (baked into every request)
  - Reasoning effort (low / medium / high)

- **Usage tracking** -- Per-key analytics dashboard showing total requests, success rate, average latency, output volume, and a 14-day daily breakdown.

- **Grading sessions** -- Built-in grading feature for managing and tracking assignment grading workflows.

- **Chrome extension for automatic session sync** -- A Manifest V3 Chrome extension that watches for cookie changes on `aichat.uva.nl` and automatically syncs your session to the proxy. No more manually copying cookies from DevTools every time they expire.

## How it works

The project was built by reverse-engineering UvA's AI Chat frontend:

1. **Source map extraction** -- UvA's Next.js deployment ships public source maps at `/_next/static/chunks/{hash}.js.map`. The original TypeScript source was recovered from these maps, revealing the full API surface, request formats, and authentication flow.

2. **Request format discovery** -- The upstream platform uses Vercel AI SDK v2 with a custom SSE format. Each request requires fresh UUID v4 identifiers for both the thread and message. The proxy generates these automatically.

3. **Authentication chain** -- Users authenticate via Azure AD (UvA's SSO) through NextAuth.js. The session cookies from `aichat.uva.nl` are stored per-user and injected into upstream requests. The Chrome extension automates this cookie capture.

4. **Translation layer** -- The proxy translates between standard OpenAI API format and UvA's internal format:
   - OpenAI messages array becomes UvA's `{id, message, flags, overrides}` structure
   - UvA's `{"type":"text-delta","delta":"..."}` SSE events are translated to OpenAI's `data: {"choices":[{"delta":{"content":"..."}}]}` format
   - Tool definitions are converted to XML system prompts, and XML tool-call blocks in model output are parsed back into OpenAI function call objects

5. **State persistence** -- For multi-turn Responses API conversations, conversation state is persisted in Supabase using `previous_response_id` chaining, matching OpenAI's stateful conversation model.

## Architecture

```
External tool (Claude Code, Codex, etc.)
    |
    | Standard OpenAI API format
    v
UvA AI Proxy (Next.js on Vercel)
    |
    | Translates to UvA format, injects session cookies
    v
aichat.uva.nl (UvA's AI platform)
    |
    | Vercel AI SDK v2 SSE stream
    v
Proxy translates back to OpenAI format
    |
    v
Tool receives standard OpenAI response
```

**Stack:** Next.js 15 (App Router, Turbopack), Bun, Supabase (Postgres), NextAuth.js (Azure AD), deployed on Vercel.

## Setup

### Prerequisites

- [Bun](https://bun.sh) installed
- A Supabase project (free tier works)
- Azure AD credentials (UvA tenant)

### Quick start

```bash
# Clone and install
git clone https://github.com/tr4m0ryp/UVA_AI_V2.git
cd UVA_AI_V2

# Copy env template and fill in your credentials
cp .env.example .env

# Run (installs deps, validates env, starts dev server)
./run
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

Run the migrations in `supabase/migrations/` against your Supabase project (via the Supabase dashboard SQL editor or the CLI).

### Chrome extension

1. Open `chrome://extensions` and enable Developer Mode
2. Click "Load unpacked" and select the `extension/` directory
3. Click the extension icon, enter your proxy URL and an API key from the dashboard
4. Log into `aichat.uva.nl` -- cookies sync automatically

## Usage with external tools

Once you have a running proxy and an API key from the dashboard:

### Claude Code

```bash
# Set the API base to your proxy
export ANTHROPIC_BASE_URL=https://your-proxy.vercel.app/api/v1
export OPENAI_API_KEY=uva-your-api-key-here

claude --model gpt-5
```

### OpenAI Codex CLI

```bash
export OPENAI_BASE_URL=https://your-proxy.vercel.app/api/v1
export OPENAI_API_KEY=uva-your-api-key-here

codex
```

### Generic OpenAI client

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

## API endpoints

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/api/v1/chat/completions` | OpenAI-compatible streaming chat completions |
| `POST` | `/api/v1/responses` | Responses API with tool-call support |
| `GET` | `/api/v1/models` | List available models |
| `PUT` | `/api/v1/session` | Update UvA session cookie (used by Chrome extension) |

## Disclaimer

This project is for educational and research purposes. It relies on reverse-engineering a university platform and may break if UvA changes their internal API. Use responsibly and in accordance with your institution's acceptable use policies.
