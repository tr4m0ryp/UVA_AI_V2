#!/bin/bash
#
# Launch all UvA AI Chat services:
#   1. C proxy           on :8787 (API + dashboard)
#   2. Open WebUI API    on :8080 (FastAPI backend)
#   3. Open WebUI UI     on :5173 (SvelteKit dev server)
#   4. opencode serve    on :4096 (AI coding assistant server, optional)
#   5. opencode-web      on :5174 (opencode UI, Vite dev server)
#
# Press Ctrl+C to stop all services.

ROOT="$(cd "$(dirname "$0")" && pwd)"
PROXY_DIR="$ROOT/services/proxy"
WEBUI_DIR="$ROOT/vendor/open-webui"
OC_WEB_DIR="$ROOT/vendor/opencode-web"
VENV_DIR="$WEBUI_DIR/.venv"
OPENCODE="$HOME/.opencode/bin/opencode"

PIDS=()

cleanup() {
    echo ""
    echo "[start-all] Shutting down..."
    for pid in "${PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    wait 2>/dev/null || true
    echo "[start-all] All services stopped."
    exit 0
}

trap cleanup INT TERM

# --- Validate prerequisites ---
if [ ! -f "$PROXY_DIR/proxy.env" ]; then
    echo "[start-all] ERROR: proxy.env not found at $PROXY_DIR/proxy.env"
    echo "[start-all] Copy proxy.env.example to proxy.env and fill in your session cookie."
    exit 1
fi

# Export proxy API key - falls through to global session cookie for non-prefixed values
export UVA_PROXY_API_KEY="${UVA_PROXY_API_KEY:-uva-local}"

# --- 1. C Proxy ---
echo "[start-all] Building and starting C proxy on :8787..."
make -C "$PROXY_DIR" -j"$(nproc)" 2>&1
(
    cd "$PROXY_DIR"
    ./uva-proxy --port 8787 --headless
) &
PIDS+=($!)
echo "[start-all] C proxy started (pid ${PIDS[-1]})"

# --- 2. Open WebUI FastAPI backend ---
echo "[start-all] Starting Open WebUI backend on :8080..."

UVICORN="$VENV_DIR/bin/uvicorn"
VENV_PYTHON="$VENV_DIR/bin/python3"

# Validate venv: recreate if missing or if the interpreter no longer exists
if [ ! -x "$VENV_PYTHON" ] || ! "$VENV_PYTHON" -c '' 2>/dev/null; then
    echo "[start-all] Venv missing or stale, rebuilding with uv + Python 3.12..."
    rm -rf "$VENV_DIR"
    if ! uv venv "$VENV_DIR" --python 3.12; then
        echo "[start-all] ERROR: uv venv failed. Install uv: https://github.com/astral-sh/uv"
        cleanup
    fi
fi

# Auto-install open-webui packages (one-time, idempotent)
if ! "$VENV_PYTHON" -c "import open_webui" 2>/dev/null; then
    echo "[start-all] Installing open-webui Python package (first run, this takes a few minutes)..."
    if ! uv pip install -e "$WEBUI_DIR" --python "$VENV_PYTHON" --quiet; then
        echo "[start-all] ERROR: open-webui install failed"
        cleanup
    fi
    echo "[start-all] open-webui installed."
fi

(
    cd "$WEBUI_DIR/backend"
    export DATA_DIR="$WEBUI_DIR/backend/data"
    mkdir -p "$DATA_DIR"
    if [ -f "$WEBUI_DIR/.env" ]; then
        set -a
        source "$WEBUI_DIR/.env"
        set +a
    fi
    "$UVICORN" open_webui.main:app --host 0.0.0.0 --port "${PORT:-8080}" --forwarded-allow-ips '*' 2>&1
) &
PIDS+=($!)
echo "[start-all] Open WebUI backend started (pid ${PIDS[-1]})"

# --- 3. SvelteKit dev server ---
echo "[start-all] Starting SvelteKit dev server on :5173..."
(
    cd "$WEBUI_DIR"
    npm run dev 2>&1
) &
PIDS+=($!)
echo "[start-all] SvelteKit dev server started (pid ${PIDS[-1]})"

# --- 4. opencode server (optional) ---
if [ -x "$OPENCODE" ]; then
    echo "[start-all] Starting opencode server on :4096..."
    (
        cd "$ROOT"
        "$OPENCODE" serve 2>&1
    ) &
    PIDS+=($!)
    echo "[start-all] opencode server started (pid ${PIDS[-1]})"
else
    echo "[start-all] opencode not found at $OPENCODE - skipping."
    echo "[start-all]   To install: curl -fsSL https://opencode.ai/install | sh"
fi

# --- 5. opencode-web Vite dev server ---
echo "[start-all] Starting opencode-web on :5174..."
(
    cd "$OC_WEB_DIR"
    bun run dev 2>&1
) &
PIDS+=($!)
echo "[start-all] opencode-web started (pid ${PIDS[-1]})"

echo ""
echo "============================================="
echo "  UvA AI Chat services running:"
echo "    Proxy dashboard:   http://127.0.0.1:8787/dashboard"
echo "    Open WebUI API:    http://127.0.0.1:8080"
echo "    Chat interface:    http://127.0.0.1:5173"
if [ -x "$OPENCODE" ]; then
echo "    opencode server:   http://127.0.0.1:4096"
fi
echo "    opencode-web:      http://127.0.0.1:5174"
echo "============================================="
echo ""
echo "Press Ctrl+C to stop all services."
echo ""

wait
