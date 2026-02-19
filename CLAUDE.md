# Coding Rules

## CRITICAL: NO EMOJIS - EVER
**This is the most important rule and must be followed without exception.**

- ABSOLUTELY NO emojis in any file, ever
- This includes source code, documentation, comments, test files, configuration files,
  log messages, error messages, conversational responses, commit messages, ANY text output
- Use descriptive plain text instead
- Before outputting ANY text, verify it contains NO emojis

## Primary Language
- **C** is the primary programming language for this project
- Use other languages only when strictly necessary (e.g., shell scripts for build tooling)

## File Conventions
- **NEVER create `.md` files** except this `CLAUDE.md`
- **Code should be self-documenting** via clear naming and comments
- **Token efficiency is critical** -- every unnecessary file wastes context window

## File Size Limit (STRICT)
- **Maximum 300 lines per source file.** No exceptions.
- When a module approaches ~200 lines, proactively split it into subdirectory files.
- Group related functions into logically named subfiles.
- Use internal headers (`*_internal.h`) to share state across split files within a module.
- Re-export public items from existing headers so external callers don't change.

## Project Structure

```
services/proxy/
  Makefile                         # Auto-discovers src/**/*.c via find
  run                              # Run script
  test_chat.sh                     # Chat endpoint test script
  include/
    apikey.h                       # API key generation/validation
    auth.h                         # UvA session auth
    auth_internal.h                # Internal auth helpers
    browser_monitor.h              # Browser cookie monitor
    config.h                       # Configuration (.env parsing)
    dashboard.h                    # Dashboard HTTP routes
    dashboard_internal.h           # Internal dashboard helpers
    database.h                     # SQLite database operations
    database_internal.h            # Internal database helpers
    server.h                       # HTTP server
    sqlite3.h                      # SQLite3 amalgamation header
    stream.h                       # SSE stream parser/emitter
    translator.h                   # OpenAI <-> UvA format translation
    upstream.h                     # UvA upstream API client
  src/
    main.c                         # Entry point
    apikey/
      generate.c                   # API key generation (SHA-256 hashed)
      validate.c                   # API key validation
    auth/
      browser.c                    # Browser-based UvA login
      browser_monitor.c            # Chrome cookie monitoring
      chrome.c                     # Chrome cookie extraction
      session.c                    # UvA session management
    config/
      config.c                     # .env config loading
    dashboard/
      auth.c                       # Dashboard auth endpoints
      auth_browser.c               # Browser login flow endpoints
      keys.c                       # API key CRUD endpoints
      router.c                     # Dashboard HTTP router
      static.c                     # Static file serving (dashboard UI)
    database/
      database.c                   # SQLite database init/schema
      keys.c                       # Key storage operations
      users.c                      # User storage operations
    server/
      response.c                   # HTTP response helpers
      router.c                     # Main HTTP router
      server.c                     # HTTP server (listen/accept)
    stream/
      emitter.c                    # SSE stream emitter (OpenAI format)
      parser.c                     # SSE stream parser (UvA format)
    translator/
      messages.c                   # Message format translation
      models.c                     # Model name mapping
    upstream/
      actions.c                    # UvA server action calls
      chat.c                       # Chat completions proxy
      client.c                     # UvA HTTP client
  dashboard/
    index.html                     # Vanilla HTML/CSS/JS dashboard UI
    css/                           # Stylesheets (style, sidebar, grading, auth)
    img/                           # UvA logo SVG
    js/                            # Client-side JS modules
```

## Dependencies
- **libcurl** -- HTTP client (UvA upstream, dashboard API)
- **json-c** -- JSON parsing and generation
- **OpenSSL** -- Cryptographic operations (SHA-256, key hashing)
- **SQLite3** -- Local database for users, API keys, sessions
- **pthreads** -- Threading
- **Standard C99** -- POSIX extensions via `_GNU_SOURCE`

## Key Architecture Notes
- The app is an OpenAI-compatible proxy to UvA's AI chat platform (aichat.uva.nl)
- C HTTP server listens on a port, serves dashboard static files and API routes
- Dashboard at `/dashboard/` is vanilla HTML/CSS/JS with UvA red branding
- `/v1/chat/completions` translates OpenAI format to UvA's format and streams SSE back
- `/v1/models` returns available models mapped from UvA's model list
- API keys are SHA-256 hashed and stored in SQLite
- UvA session cookies are obtained via browser login flow or Chrome cookie extraction
- UvA's upstream API uses Vercel AI SDK v2 SSE format (`{"type":"text-delta","delta":"..."}`)
- Each UvA request needs a fresh UUID v4 for both thread ID and message ID

## Build & Run
```bash
cd services/proxy
make            # builds uva-proxy binary
./run           # or ./uva-proxy
```
