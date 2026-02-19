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
proxy/
  Makefile                         # Auto-discovers src/**/*.c via find
  run                              # Lightweight single-service dev launcher
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
    github.h                       # GitHub OAuth + API
    grading.h                      # Assignment grading sessions
    project.h                      # Cloud coding project management
    responses.h                    # OpenAI Responses API
    responses_internal.h           # Internal responses helpers
    server.h                       # HTTP server
    sqlite3.h                      # SQLite3 amalgamation header
    stream.h                       # SSE stream parser/emitter
    terminal.h                     # WebSocket terminal
    translator.h                   # OpenAI <-> UvA format translation
    upstream.h                     # UvA upstream API client
    vps.h                          # VPS/Docker execution layer
    websocket.h                    # WebSocket framing / handshake
    webui.h                        # Open WebUI lifecycle management
    webui_internal.h               # Internal webui helpers
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
      chat.c                       # Dashboard chat endpoint
      github.c                     # GitHub integration endpoints
      grading.c                    # Grading session endpoints
      keys.c                       # API key CRUD endpoints
      projects.c                   # Cloud coding project endpoints
      router.c                     # Dashboard HTTP router
      static.c                     # Static file serving (dashboard UI)
      terminal.c                   # WebSocket terminal endpoint
    database/
      database.c                   # SQLite database init/schema
      grading.c                    # Grading session storage
      keys.c                       # Key storage operations
      logs.c                       # Request log storage
      projects.c                   # Project storage operations
      users.c                      # User storage operations
    github/
      api.c                        # GitHub REST API client
      oauth.c                      # GitHub OAuth flow
    platform/
      common.c                     # Cross-platform utilities
      linux.c                      # Linux-specific (pty, paths)
      macos.c                      # macOS-specific
      windows.c                    # Windows-specific (Winsock)
    responses/
      emitter.c                    # Responses API SSE emitter
      handler.c                    # Responses API request handler
      input.c                      # Input parsing
      route.c                      # Route entry point
      state.c                      # Multi-turn state persistence
      tools.c                      # Tool call parsing/emission
    server/
      response.c                   # HTTP response helpers
      router.c                     # Main HTTP router
      server.c                     # HTTP server (listen/accept)
    stream/
      emitter.c                    # SSE stream emitter (OpenAI format)
      parser.c                     # SSE stream parser (UvA format)
    terminal/
      bridge.c                     # WebSocket <-> PTY bridge
      spawn.c                      # PTY process spawning
    translator/
      messages.c                   # Message format translation
      models.c                     # Model name mapping
    upstream/
      actions.c                    # UvA server action calls
      chat.c                       # Chat completions proxy
      client.c                     # UvA HTTP client
    vps/
      docker.c                     # Docker container management
      exec.c                       # VPS command execution via SSH
    websocket/
      frames.c                     # WebSocket frame encode/decode
      handshake.c                  # WebSocket upgrade handshake
    webui/
      health.c                     # Open WebUI health check
      launcher.c                   # Launch/stop Open WebUI process
      process.c                    # Process lifecycle management
      setup.c                      # One-time Open WebUI setup
  dashboard/
    index.html                     # Vanilla HTML/CSS/JS dashboard UI
    css/                           # Stylesheets (style, sidebar, grading, coding, auth)
    img/                           # UvA logo SVG
    js/                            # Client-side JS modules
    vendor/
      pdfjs/                       # PDF.js (grading file upload)
      xterm/                       # xterm.js (terminal UI)

docker/
  Dockerfile                       # Docker image for the proxy
  entrypoint.sh                    # Container entrypoint
  setup-vps.sh                     # VPS bootstrap script

start.sh                           # One-shot launcher for all services
opencode.json                      # opencode provider/server config
vendor/
  open-webui/                      # Open WebUI submodule (chat frontend)
  opencode-web/                    # opencode-web submodule (cloud coding UI)
  codex/                           # OpenAI Codex CLI submodule
docs/
  analysis.txt                     # Reverse engineering analysis notes
  screenshots/                     # README screenshots
```

## Dependencies
- **libcurl** -- HTTP client (UvA upstream, dashboard API)
- **json-c** -- JSON parsing and generation
- **OpenSSL** -- Cryptographic operations (SHA-256, key hashing)
- **SQLite3** -- Local database (users, API keys, sessions, grading, projects)
- **pthreads** -- Threading
- **Standard C99** -- POSIX extensions via `_GNU_SOURCE`

## Key Architecture Notes
- The app is an OpenAI-compatible proxy to UvA's AI chat platform (aichat.uva.nl)
- C HTTP server listens on a port (default 8787), serves dashboard static files and API routes
- Dashboard at `/dashboard/` is vanilla HTML/CSS/JS with UvA red branding
- `/v1/chat/completions` translates OpenAI format to UvA's format and streams SSE back
- `/v1/responses` implements OpenAI Responses API with tool-call support and state persistence
- `/v1/models` returns available models mapped from UvA's model list
- API keys are SHA-256 hashed and stored in SQLite
- UvA session cookies are obtained via browser login flow or Chrome cookie extraction
- UvA's upstream API uses Vercel AI SDK v2 SSE format (`{"type":"text-delta","delta":"..."}`)
- Each UvA request needs a fresh UUID v4 for both thread ID and message ID
- WebSocket terminal mounts a PTY for cloud coding projects
- GitHub OAuth enables pulling repos into cloud coding projects
- Open WebUI is launched as a subprocess and health-checked before serving

## Build & Run
```bash
# All services (recommended):
./start.sh

# Proxy only:
cd proxy
make            # builds uva-proxy binary
./run           # or ./uva-proxy --port 8787
```
