#!/bin/bash
# =============================================================================
#  UvA AI Chat -- one-shot launcher
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

# -- colours -----------------------------------------------------------------
RED='\033[0;31m'; YEL='\033[1;33m'; GRN='\033[0;32m'
CYN='\033[0;36m'; BLD='\033[1m'; RST='\033[0m'

log()  { echo -e "${CYN}[start]${RST} $*"; }
ok()   { echo -e "${GRN}[  ok ]${RST} $*"; }
warn() { echo -e "${YEL}[warn ]${RST} $*"; }
die()  { echo -e "${RED}[error]${RST} $*" >&2; stop_all; exit 1; }

PIDS=()

# -- stop_all ----------------------------------------------------------------
stop_all() {
    echo ""
    log "Shutting down all services..."
    for pid in "${PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
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

# -- --reset: kill previous run ----------------------------------------------
reset_stack() {
    log "Resetting previous stack..."
    if [[ -f "$PIDFILE" ]]; then
        while IFS= read -r pid; do
            kill "$pid" 2>/dev/null && log "  killed pid $pid" || true
        done < "$PIDFILE"
        rm -f "$PIDFILE"
    fi
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
    [[ "${2:-}" == "--only" ]] && { trap - EXIT; exit 0; }
fi

# -- helper: check binary present -------------------------------------------
need() {
    command -v "$1" &>/dev/null || die "'$1' not found. $2"
}

# ============================================================================
#  STEP 0: Install system dependencies
# ============================================================================
install_system_deps() {
    log "Checking system dependencies..."

    # Detect package manager
    local PM=""
    local INSTALL_CMD=""
    if command -v apt-get &>/dev/null; then
        PM="apt"
        INSTALL_CMD="sudo apt-get install -y"
    elif command -v dnf &>/dev/null; then
        PM="dnf"
        INSTALL_CMD="sudo dnf install -y"
    elif command -v yum &>/dev/null; then
        PM="yum"
        INSTALL_CMD="sudo yum install -y"
    elif command -v pacman &>/dev/null; then
        PM="pacman"
        INSTALL_CMD="sudo pacman -S --noconfirm"
    elif command -v brew &>/dev/null; then
        PM="brew"
        INSTALL_CMD="brew install"
    else
        warn "Could not detect package manager. Install these manually if build fails:"
        warn "  gcc, make, curl, libcurl (dev), json-c (dev), openssl (dev), sqlite3, python3, nodejs/npm"
        return 0
    fi

    # Build the list of packages to install based on what is missing
    local PKGS=()

    case "$PM" in
        apt)
            command -v gcc   &>/dev/null || PKGS+=(build-essential)
            command -v make  &>/dev/null || PKGS+=(make)
            command -v curl  &>/dev/null || PKGS+=(curl)
            pkg-config --exists libcurl 2>/dev/null || PKGS+=(libcurl4-openssl-dev)
            pkg-config --exists json-c  2>/dev/null || PKGS+=(libjson-c-dev)
            pkg-config --exists openssl 2>/dev/null || PKGS+=(libssl-dev)
            command -v sqlite3  &>/dev/null || PKGS+=(sqlite3)
            command -v python3  &>/dev/null || PKGS+=(python3)
            command -v npm      &>/dev/null || PKGS+=(npm)
            command -v git      &>/dev/null || PKGS+=(git)
            ;;
        dnf|yum)
            command -v gcc   &>/dev/null || PKGS+=(gcc)
            command -v make  &>/dev/null || PKGS+=(make)
            command -v curl  &>/dev/null || PKGS+=(curl)
            pkg-config --exists libcurl 2>/dev/null || PKGS+=(libcurl-devel)
            pkg-config --exists json-c  2>/dev/null || PKGS+=(json-c-devel)
            pkg-config --exists openssl 2>/dev/null || PKGS+=(openssl-devel)
            command -v sqlite3  &>/dev/null || PKGS+=(sqlite)
            command -v python3  &>/dev/null || PKGS+=(python3)
            command -v npm      &>/dev/null || PKGS+=(npm)
            command -v git      &>/dev/null || PKGS+=(git)
            command -v pkg-config &>/dev/null || PKGS+=(pkgconf-pkg-config)
            ;;
        pacman)
            command -v gcc   &>/dev/null || PKGS+=(base-devel)
            command -v curl  &>/dev/null || PKGS+=(curl)
            pkg-config --exists libcurl 2>/dev/null || PKGS+=(curl)
            pkg-config --exists json-c  2>/dev/null || PKGS+=(json-c)
            pkg-config --exists openssl 2>/dev/null || PKGS+=(openssl)
            command -v sqlite3  &>/dev/null || PKGS+=(sqlite)
            command -v python3  &>/dev/null || PKGS+=(python)
            command -v npm      &>/dev/null || PKGS+=(npm)
            command -v git      &>/dev/null || PKGS+=(git)
            ;;
        brew)
            command -v gcc   &>/dev/null || true  # Xcode CLT provides this
            pkg-config --exists libcurl 2>/dev/null || PKGS+=(curl)
            pkg-config --exists json-c  2>/dev/null || PKGS+=(json-c)
            pkg-config --exists openssl 2>/dev/null || PKGS+=(openssl)
            command -v sqlite3  &>/dev/null || PKGS+=(sqlite)
            command -v python3  &>/dev/null || PKGS+=(python3)
            command -v npm      &>/dev/null || PKGS+=(npm)
            command -v git      &>/dev/null || PKGS+=(git)
            ;;
    esac

    if [[ ${#PKGS[@]} -eq 0 ]]; then
        ok "All system dependencies already installed."
        return 0
    fi

    log "Installing missing packages: ${PKGS[*]}"
    if [[ "$PM" == "apt" ]]; then
        sudo apt-get update -qq
    fi
    $INSTALL_CMD "${PKGS[@]}" || die "Package installation failed. Install manually: ${PKGS[*]}"
    ok "System dependencies installed."
}

install_system_deps

# ============================================================================
#  STEP 0b: Initialize git submodules (fresh clone support)
# ============================================================================
log "Checking git submodules..."
if [[ -f "$ROOT/.gitmodules" ]]; then
    # Check if any submodule is empty (not initialized)
    _needs_init=0
    for _subdir in "$WEBUI_DIR" "$OC_WEB_DIR"; do
        if [[ ! -d "$_subdir/.git" ]] && [[ ! -f "$_subdir/.git" ]]; then
            _needs_init=1
            break
        fi
        # Also check if the directory is essentially empty (no source files)
        if [[ -d "$_subdir" ]] && [[ -z "$(ls -A "$_subdir" 2>/dev/null)" ]]; then
            _needs_init=1
            break
        fi
    done

    if [[ "$_needs_init" -eq 1 ]]; then
        log "Initializing git submodules (first clone)..."
        (cd "$ROOT" && git submodule update --init --recursive 2>&1 | tail -10) \
            || die "git submodule init failed. Run manually: git submodule update --init --recursive"
        ok "Submodules initialized."
    else
        ok "Submodules already initialized."
    fi
fi

# ============================================================================
#  STEP 0c: Install runtime tools (uv, bun, opencode)
# ============================================================================
log "Checking runtime tools..."

# uv (Python venv manager)
if ! command -v uv &>/dev/null; then
    warn "'uv' not found -- installing via curl..."
    curl -fsSL https://astral.sh/uv/install.sh | sh
    export PATH="$HOME/.local/bin:$PATH"
    command -v uv &>/dev/null || die "uv install failed. Try manually: https://github.com/astral-sh/uv"
    ok "uv installed."
fi

# bun (opencode-web dev server)
if ! command -v bun &>/dev/null; then
    warn "'bun' not found -- installing via curl..."
    curl -fsSL https://bun.sh/install | bash
    export PATH="$HOME/.bun/bin:$PATH"
    command -v bun &>/dev/null || die "bun install failed. Try manually: https://bun.sh"
    ok "bun installed."
fi

# opencode binary
if [[ ! -x "$OPENCODE" ]]; then
    warn "opencode not found at $OPENCODE -- installing..."
    curl -fsSL https://opencode.ai/install | sh
    command -v opencode &>/dev/null && OPENCODE="$(command -v opencode)"
    [[ -x "$OPENCODE" ]] || { warn "opencode install failed -- skipping opencode server."; OPENCODE=""; }
    [[ -n "$OPENCODE" ]] && ok "opencode installed at $OPENCODE."
fi

# proxy.env -- create from example if missing
if [[ ! -f "$PROXY_DIR/proxy.env" ]]; then
    warn "proxy.env not found -- creating from example"
    cp "$PROXY_DIR/proxy.env.example" "$PROXY_DIR/proxy.env"
    ok "proxy.env created at $PROXY_DIR/proxy.env"
fi
export UVA_PROXY_API_KEY="${UVA_PROXY_API_KEY:-uva-local}"

# Detect whether a session cookie is already configured
_cookie_configured=0
if grep -qE '^UVA_SESSION_COOKIE=".+"' "$PROXY_DIR/proxy.env" 2>/dev/null; then
    _cookie_configured=1
fi

ok "All prerequisites satisfied."
echo ""

mkdir -p "$ROOT/logs"

# ============================================================================
#  STEP 1: Build and start the C proxy
# ============================================================================
log "Building C proxy..."
make -C "$PROXY_DIR" -j"$(nproc)" 2>&1 | sed 's/^/  [make] /' \
    || die "Proxy build failed -- check compiler output above."
ok "C proxy built."

# When no cookie is configured, start WITHOUT --headless so the proxy opens
# the dashboard in app mode.  The dashboard has a "Login with UvA Account"
# button that triggers the automated browser login (opens aichat.uva.nl,
# monitors browser cookie databases, and captures the session automatically).
#
# When a cookie already exists, start with --headless (no GUI window needed).
if [[ "$_cookie_configured" -eq 1 ]]; then
    log "Starting C proxy on :8787 (headless -- session cookie found)..."
    (
        cd "$PROXY_DIR"
        exec ./uva-proxy --port 8787 --headless
    ) &>> "$ROOT/logs/proxy.log" &
else
    log "Starting C proxy on :8787 (browser login -- no session cookie)..."
    (
        cd "$PROXY_DIR"
        exec ./uva-proxy --port 8787
    ) &>> "$ROOT/logs/proxy.log" &
fi
PIDS+=($!)
ok "C proxy started (pid ${PIDS[-1]})"

# Wait for the proxy to become healthy before continuing
log "Waiting for proxy health check..."
_proxy_ready=0
for _i in $(seq 1 30); do
    sleep 0.5
    if curl -sf http://127.0.0.1:8787/health >/dev/null 2>&1; then
        _proxy_ready=1
        break
    fi
done
if [[ "$_proxy_ready" -eq 0 ]]; then
    warn "Proxy did not respond to health check within 15s -- continuing anyway."
    warn "Check logs/proxy.log for errors."
fi

# If no session cookie, tell the user about the automated browser login
if [[ "$_cookie_configured" -eq 0 ]]; then
    echo ""
    echo -e "${BLD}${YEL}============================================================${RST}"
    echo -e "${BLD}  First-time setup: login required${RST}"
    echo -e "${YEL}============================================================${RST}"
    echo -e "  The dashboard should open automatically in your browser."
    echo -e "  Click ${BLD}'Login with UvA Account'${RST} to authenticate."
    echo -e ""
    echo -e "  This opens aichat.uva.nl where you log in with your UvA"
    echo -e "  credentials.  The session cookie is captured automatically"
    echo -e "  -- no manual copy-pasting needed."
    echo -e ""
    echo -e "  If the browser did not open, visit:"
    echo -e "  ${BLD}http://127.0.0.1:8787/dashboard${RST}"
    echo -e "${YEL}============================================================${RST}"
    echo ""
fi

# ============================================================================
#  STEP 2: Open WebUI Python backend
# ============================================================================
log "Setting up Open WebUI Python environment..."

# Rebuild venv if missing or broken
if [[ ! -x "$VENV_PYTHON" ]] || ! "$VENV_PYTHON" -c '' 2>/dev/null; then
    warn "venv missing or stale -- rebuilding with Python 3.12..."
    rm -rf "$VENV_DIR"
    uv venv "$VENV_DIR" --python 3.12 \
        || die "uv venv failed. Make sure Python 3.12 is available: uv python install 3.12"
    ok "venv created."
fi

# Install open-webui package if not present
if ! "$VENV_PYTHON" -c "import open_webui" 2>/dev/null; then
    log "Installing open-webui Python package (first run -- a few minutes)..."
    uv pip install -e "$WEBUI_DIR" --python "$VENV_PYTHON" \
        || die "open-webui pip install failed."
    ok "open-webui installed."
fi

# Build frontend if needed
if [[ ! -f "$WEBUI_DIR/build/index.html" ]]; then
    log "Building Open WebUI frontend (first run)..."
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

log "Starting Open WebUI on :8080..."
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

# ============================================================================
#  STEP 3: opencode server
# ============================================================================
if [[ -x "$OPENCODE" ]]; then
    log "Starting opencode server on :4096..."
    (
        cd "$ROOT"
        exec "$OPENCODE" serve
    ) &>> "$ROOT/logs/opencode.log" &
    PIDS+=($!)
    ok "opencode server started (pid ${PIDS[-1]})"
else
    warn "opencode not available -- skipping opencode server."
fi

# ============================================================================
#  STEP 4: opencode-web Vite dev server
# ============================================================================
log "Setting up opencode-web..."
if [[ ! -d "$OC_WEB_DIR/node_modules" ]]; then
    log "Installing opencode-web dependencies..."
    (cd "$OC_WEB_DIR" && bun install 2>&1 | tail -5) \
        || die "bun install for opencode-web failed."
    ok "opencode-web dependencies installed."
fi

log "Starting opencode-web on :5174..."
(
    cd "$OC_WEB_DIR"
    exec bun run dev --port 5174 --host 0.0.0.0
) &>> "$ROOT/logs/opencode-web.log" &
PIDS+=($!)
ok "opencode-web started (pid ${PIDS[-1]})"

# -- Save PIDs for future --reset -------------------------------------------
printf '%s\n' "${PIDS[@]}" > "$PIDFILE"

# ============================================================================
#  Summary -- final output directs user to the dashboard
# ============================================================================
DASHBOARD_URL="http://127.0.0.1:8787/dashboard"
echo ""
echo -e "${BLD}${GRN}============================================================${RST}"
echo -e "${BLD}  UvA AI Chat stack is running${RST}"
echo -e "${GRN}============================================================${RST}"
echo -e "  ${BLD}Dashboard${RST}             ${DASHBOARD_URL}"
echo -e "  ${BLD}Chat interface${RST}        http://127.0.0.1:8080"
if [[ -x "$OPENCODE" ]]; then
echo -e "  ${BLD}opencode server${RST}       http://127.0.0.1:4096"
fi
echo -e "  ${BLD}opencode-web UI${RST}       http://127.0.0.1:5174"
echo ""
echo -e "  ${BLD}OpenAI-compatible API${RST}  http://127.0.0.1:8787/v1"
echo ""
echo -e "  Logs: $ROOT/logs/"
echo -e "  PIDs: $PIDFILE"
echo -e "${GRN}============================================================${RST}"
echo ""
echo -e "  Open ${BLD}${DASHBOARD_URL}${RST} to get started."
echo ""
echo -e "  Press ${BLD}Ctrl+C${RST} to stop all services."
echo ""

# Disable EXIT trap (we only want cleanup on SIGINT/SIGTERM from here)
trap - EXIT

# Wait for any child to exit, then shut everything down
wait
