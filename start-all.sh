#!/bin/bash
#
# Launch all three UvA AI Chat services:
#   1. C proxy         on :8787 (API + dashboard)
#   2. Open WebUI API  on :8080 (FastAPI backend)
#   3. Open WebUI UI   on :5173 (SvelteKit dev server)
#
# Press Ctrl+C to stop all services.

set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
PROXY_DIR="$ROOT/proxy"
WEBUI_DIR="$ROOT/open-webui"
VENV_DIR="$WEBUI_DIR/.venv"

PIDS=()

cleanup() {
    echo ""
    echo "[start-all] Shutting down..."
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null
        fi
    done
    wait 2>/dev/null
    echo "[start-all] All services stopped."
    exit 0
}

trap cleanup INT TERM

# --- 1. C Proxy ---
echo "[start-all] Starting C proxy on :8787..."
cd "$PROXY_DIR"
make -j"$(nproc)" 2>&1
./uva-proxy --port 8787 &
PIDS+=($!)
echo "[start-all] C proxy started (pid ${PIDS[-1]})"

# --- 2. Open WebUI FastAPI backend ---
echo "[start-all] Starting Open WebUI backend on :8080..."
cd "$WEBUI_DIR"

if [ ! -d "$VENV_DIR" ]; then
    echo "[start-all] ERROR: Python venv not found at $VENV_DIR"
    echo "[start-all] Run: cd open-webui && python3 -m venv .venv && source .venv/bin/activate && pip install -r backend/requirements.txt"
    cleanup
fi

(
    source "$VENV_DIR/bin/activate"
    cd "$WEBUI_DIR/backend"
    export DATA_DIR="$WEBUI_DIR/backend/data"
    mkdir -p "$DATA_DIR"
    # Load .env if present
    if [ -f "$WEBUI_DIR/.env" ]; then
        set -a
        source "$WEBUI_DIR/.env"
        set +a
    fi
    uvicorn open_webui.main:app --host 0.0.0.0 --port "${PORT:-8080}" --forwarded-allow-ips '*' 2>&1
) &
PIDS+=($!)
echo "[start-all] Open WebUI backend started (pid ${PIDS[-1]})"

# --- 3. SvelteKit dev server ---
echo "[start-all] Starting SvelteKit dev server on :5173..."
cd "$WEBUI_DIR"
npm run dev 2>&1 &
PIDS+=($!)
echo "[start-all] SvelteKit dev server started (pid ${PIDS[-1]})"

echo ""
echo "============================================="
echo "  UvA AI Chat services running:"
echo "    Proxy dashboard:  http://127.0.0.1:8787/dashboard"
echo "    Open WebUI API:   http://127.0.0.1:8080"
echo "    Chat interface:   http://127.0.0.1:5173"
echo "============================================="
echo ""
echo "Press Ctrl+C to stop all services."
echo ""

# Wait for any child to exit
wait
