# Coding Rules

## CRITICAL: NO EMOJIS - EVER
**This is the most important rule and must be followed without exception.**

- ABSOLUTELY NO emojis in any file, ever
- This includes source code, documentation, comments, test files, configuration files,
  log messages, error messages, conversational responses, commit messages, ANY text output
- Use descriptive plain text instead
- Before outputting ANY text, verify it contains NO emojis

## Primary Language
- **TypeScript** is the primary language for this project
- Next.js 15 with App Router, deployed on Vercel
- Bun as the package manager and runtime
- Supabase for the database

## File Conventions
- **NEVER create `.md` files** except this `CLAUDE.md`
- **Code should be self-documenting** via clear naming and comments
- **Token efficiency is critical** -- every unnecessary file wastes context window

## Project Structure

```
reversed_uva_ai/
  .github/workflows/deploy.yml    # CI/CD: type-check, build, migrate, deploy
  package.json / bun.lock          # Dependencies
  next.config.ts / tsconfig.json   # Next.js + TS config
  auth.ts                          # NextAuth.js config (Azure AD)
  middleware.ts                    # Auth middleware
  app/
    layout.tsx / page.tsx          # Root layout + landing page
    globals.css                    # Global styles
    login/page.tsx                 # Login page
    dashboard/                     # Authenticated dashboard pages
      layout.tsx, page.tsx
      keys/page.tsx                # API key management
      settings/page.tsx            # User settings (UvA session cookie)
      usage/page.tsx               # Usage statistics
      grading/page.tsx             # Grading feature
    api/
      auth/[...nextauth]/route.ts  # NextAuth route handler
      auth/session/route.ts        # Session endpoint
      health/route.ts              # Health check
      keys/route.ts                # API key CRUD
      grading/route.ts             # Grading endpoints
      v1/chat/completions/route.ts # OpenAI-compatible chat completions
      v1/models/route.ts           # Model listing
      v1/responses/route.ts        # Responses API (tool use)
  lib/
    auth/
      session.ts                   # NextAuth session helpers + user CRUD
      api-key.ts                   # Bearer token resolution + request logging
    db/
      client.ts                    # Supabase client + Database types
      response-state.ts            # Response state persistence
    uva/
      models.ts                    # Model registry (UvA <-> OpenAI mapping)
      translator.ts                # OpenAI -> UvA request format translation
      upstream.ts                  # UvA SSE stream handling
      tools.ts                     # Tool call parsing + injection
    responses/
      emitter.ts                   # Responses API SSE event encoding
      handler.ts                   # Responses API orchestration (text + tool paths)
  supabase/
    migrations/                    # Database migrations (run via CI)
```

## Dependencies
- **next** / **react** -- App framework
- **next-auth** -- Azure AD OAuth
- **@supabase/supabase-js** -- Database client
- **uuid** -- UUID v4 generation for UvA API requests

## Key Architecture Notes
- The app is an OpenAI-compatible proxy to UvA's AI chat platform (aichat.uva.nl)
- Users authenticate via Azure AD, then create API keys in the dashboard
- API keys are hashed (SHA-256) and stored in Supabase; the raw key is shown only once
- The `/v1/chat/completions` endpoint translates OpenAI format to UvA's format and streams back
- The `/v1/responses` endpoint implements OpenAI's Responses API with tool-call support
- UvA's API uses Vercel AI SDK v2 SSE format (`{"type":"text-delta","delta":"..."}`)
- Each UvA request needs a fresh UUID v4 for both thread ID and message ID
