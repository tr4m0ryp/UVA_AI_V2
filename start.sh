#!/bin/bash
# =============================================================================
#  UvA AI Chat – one-shot launcher
#  Usage:  ./start.sh [--reset]
#
#  Services started:
#    1. C proxy           :8787  (API + dashboard)
#    2. Open WebUI         :8080  (backend + frontend)
#    3. opencode serve    :4096  (AI coding assistant server)
#    4. opencode-web      :5174  (Vite dev UI)
#
#  --reset   Kill any already-running instance of this stack first.
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
PROXY_DIR="$ROOT/proxy"
WEBUI_DIR="$ROOT/vendor/open-webui"
OC_WEB_DIR="$ROOT/vendor/opencode-web"
VENV_DIR="$WEBUI_DIR/.venv"
VENV_PYTHON="$VENV_DIR/bin/python3"
UVICORN="$VENV_DIR/bin/uvicorn"
OPENCODE="${OPENCODE_BIN:-$HOME/.opencode/bin/opencode}"
PIDFILE="$ROOT/.stack.pids"

# ── colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; YEL='\033[1;33m'; GRN='\033[0;32m'
CYN='\033[0;36m'; BLD='\033[1m'; RST='\033[0m'

log()  { echo -e "${CYN}[start]${RST} $*"; }
ok()   { echo -e "${GRN}[start]${RST} ✓ $*"; }
warn() { echo -e "${YEL}[start]${RST} ⚠ $*"; }
die()  { echo -e "${RED}[start]${RST} ✗ $*" >&2; stop_all; exit 1; }

PIDS=()

# ── stop_all ─────────────────────────────────────────────────────────────────
stop_all() {
    echo ""
    log "Shutting down all services…"
    # Kill PIDs we launched this session
    for pid in "${PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    # Kill any PIDs saved from a previous run
    if [[ -f "$PIDFILE" ]]; then
        while IFS= read -r pid; do
            kill "$pid" 2>/dev/null || true
        done < "$PIDFILE"
        rm -f "$PIDFILE"
    fi
    wait 2>/dev/null || true
    ok "All services stopped."
}

trap stop_all INT TERM EXIT

# ── --reset: kill previous run ───────────────────────────────────────────────
reset_stack() {
    log "Resetting previous stack…"
    # Kill by saved PID file
    if [[ -f "$PIDFILE" ]]; then
        while IFS= read -r pid; do
            kill "$pid" 2>/dev/null && log "  killed pid $pid" || true
        done < "$PIDFILE"
        rm -f "$PIDFILE"
    fi
    # Kill by process name (belt-and-suspenders)
    pkill -f "uva-proxy --port 8787"            2>/dev/null || true
    pkill -f "uvicorn open_webui.main:app"       2>/dev/null || true
    pkill -f "opencode serve"                    2>/dev/null || true
    pkill -f "vite --port 5174"                  2>/dev/null || true
    pkill -f "bun run dev --port 5174"           2>/dev/null || true
    sleep 1
    ok "Previous stack cleared."
}

if [[ "${1:-}" == "--reset" ]]; then
    reset_stack
    # If called only for reset, don't start again
    [[ "${2:-}" == "--only" ]] && { trap - EXIT; exit 0; }
fi

# ── helper: check binary present ─────────────────────────────────────────────
need() {
    command -v "$1" &>/dev/null || die "'$1' not found. $2"
}

# ── 0. Prerequisite checks ────────────────────────────────────────────────────
log "Checking prerequisites…"

need gcc   "Install build-essentials: sudo apt install build-essential"
need make  "Install make: sudo apt install make"
need curl  "Install curl: sudo apt install curl"

# uv (Python venv manager)
if ! command -v uv &>/dev/null; then
    warn "'uv' not found – installing via curl…"
    curl -fsSL https://astral.sh/uv/install.sh | sh
    export PATH="$HOME/.local/bin:$PATH"
    command -v uv &>/dev/null || die "uv install failed. Try manually: https://github.com/astral-sh/uv"
    ok "uv installed."
fi

# bun (opencode-web dev server)
if ! command -v bun &>/dev/null; then
    warn "'bun' not found – installing via curl…"
    curl -fsSL https://bun.sh/install | bash
    export PATH="$HOME/.bun/bin:$PATH"
    command -v bun &>/dev/null || die "bun install failed. Try manually: https://bun.sh"
    ok "bun installed."
fi

# opencode binary
if [[ ! -x "$OPENCODE" ]]; then
    warn "opencode not found at $OPENCODE – installing…"
    curl -fsSL https://opencode.ai/install | sh
    command -v opencode &>/dev/null && OPENCODE="$(command -v opencode)"
    [[ -x "$OPENCODE" ]] || { warn "opencode install failed – skipping opencode server."; OPENCODE=""; }
    [[ -n "$OPENCODE" ]] && ok "opencode installed at $OPENCODE."
fi

# proxy.env
if [[ ! -f "$PROXY_DIR/proxy.env" ]]; then
    die "proxy.env missing at $PROXY_DIR/proxy.env\n       Copy and fill in your session cookie:\n         cp $PROXY_DIR/proxy.env.example $PROXY_DIR/proxy.env"
fi
export UVA_PROXY_API_KEY="${UVA_PROXY_API_KEY:-uva-local}"

ok "Prerequisites OK."
echo ""

# ── 1. Build & start C proxy ──────────────────────────────────────────────────
log "Building C proxy…"
make -C "$PROXY_DIR" -j"$(nproc)" 2>&1 | sed 's/^/  [make] /' \
    || die "Proxy build failed – check compiler output above."
ok "C proxy built."

log "Starting C proxy on :8787…"
(
    cd "$PROXY_DIR"
    exec ./uva-proxy --port 8787 --headless
) &>> "$ROOT/logs/proxy.log" &
PIDS+=($!)
ok "C proxy started (pid ${PIDS[-1]})"

# ── 2. Open WebUI Python backend ──────────────────────────────────────────────
log "Setting up Open WebUI Python environment…"

# Rebuild venv if missing or broken
if [[ ! -x "$VENV_PYTHON" ]] || ! "$VENV_PYTHON" -c '' 2>/dev/null; then
    warn "venv missing or stale – rebuilding with Python 3.12…"
    rm -rf "$VENV_DIR"
    uv venv "$VENV_DIR" --python 3.12 \
        || die "uv venv failed. Make sure Python 3.12 is available: uv python install 3.12"
    ok "venv created."
fi

# Install open-webui package if not present
if ! "$VENV_PYTHON" -c "import open_webui" 2>/dev/null; then
    log "Installing open-webui Python package (first run – a few minutes)…"
    uv pip install -e "$WEBUI_DIR" --python "$VENV_PYTHON" \
        || die "open-webui pip install failed."
    ok "open-webui installed."
fi

# Build frontend if needed
if [[ ! -f "$WEBUI_DIR/build/index.html" ]]; then
    log "Building Open WebUI frontend (first run)…"
    if [[ ! -d "$WEBUI_DIR/node_modules" ]]; then
        (cd "$WEBUI_DIR" && npm install --no-audit --no-fund 2>&1 | tail -5) \
            || die "npm install for open-webui failed."
    fi
    (cd "$WEBUI_DIR" && npm run build 2>&1 | tail -10) \
        || die "npm run build for open-webui failed."
    ok "Frontend built."
else
    ok "Frontend already built."
fi

log "Starting Open WebUI on :8080…"
mkdir -p "$ROOT/logs"
(
    cd "$WEBUI_DIR/backend"
    export DATA_DIR="$WEBUI_DIR/backend/data"
    mkdir -p "$DATA_DIR"
    [[ -f "$WEBUI_DIR/.env" ]] && { set -a; source "$WEBUI_DIR/.env"; set +a; }
    exec "$UVICORN" open_webui.main:app \
        --host 0.0.0.0 --port "${PORT:-8080}" \
        --forwarded-allow-ips '*'
) &>> "$ROOT/logs/webui.log" &
PIDS+=($!)
ok "Open WebUI started (pid ${PIDS[-1]})"

# ── 3. opencode server ────────────────────────────────────────────────────────
if [[ -x "$OPENCODE" ]]; then
    log "Starting opencode server on :4096…"
    (
        cd "$ROOT"
        exec "$OPENCODE" serve
    ) &>> "$ROOT/logs/opencode.log" &
    PIDS+=($!)
    ok "opencode server started (pid ${PIDS[-1]})"
else
    warn "opencode not available – skipping opencode server."
fi

# ── 4. opencode-web Vite dev server ──────────────────────────────────────────
log "Setting up opencode-web…"
if [[ ! -d "$OC_WEB_DIR/node_modules" ]]; then
    log "Installing opencode-web dependencies…"
    (cd "$OC_WEB_DIR" && bun install 2>&1 | tail -5) \
        || die "bun install for opencode-web failed."
    ok "opencode-web dependencies installed."
fi

log "Starting opencode-web on :5174…"
(
    cd "$OC_WEB_DIR"
    exec bun run dev --port 5174 --host 0.0.0.0
) &>> "$ROOT/logs/opencode-web.log" &
PIDS+=($!)
ok "opencode-web started (pid ${PIDS[-1]})"

# ── Save PIDs for future --reset ──────────────────────────────────────────────
mkdir -p "$ROOT/logs"
printf '%s\n' "${PIDS[@]}" > "$PIDFILE"

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo -e "${BLD}${GRN}============================================================${RST}"
echo -e "${BLD}  UvA AI Chat stack is running${RST}"
echo -e "${GRN}============================================================${RST}"
echo -e "  ${BLD}Proxy dashboard${RST}   →  http://127.0.0.1:8787/dashboard"
echo -e "  ${BLD}Chat interface${RST}    →  http://127.0.0.1:8080"
if [[ -x "$OPENCODE" ]]; then
echo -e "  ${BLD}opencode server${RST}   →  http://127.0.0.1:4096"
fi
echo -e "  ${BLD}opencode-web UI${RST}   →  http://127.0.0.1:5174"
echo ""
echo -e "  ${BLD}OpenAI-compatible API${RST}  →  http://127.0.0.1:8787/v1"
echo ""
echo -e "  Logs: $ROOT/logs/"
echo -e "  PIDs: $PIDFILE"
echo -e "${GRN}============================================================${RST}"
echo ""
echo -e "  Press ${BLD}Ctrl+C${RST} to stop all services."
echo ""

# Disable EXIT trap (we only want cleanup on SIGINT/SIGTERM from here)
trap - EXIT

# Wait for any child to exit, then shut everything down
wait
