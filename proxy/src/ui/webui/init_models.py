"""Post-startup initialization for Open WebUI model icons and config.

Called by the proxy after Open WebUI is healthy. Authenticates as the
default admin user, then creates/updates workspace model entries with
correct provider logos (OpenAI, Mistral).

Usage: python3 init_models.py <webui_port>
"""
import sys
import json
import urllib.request

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
BASE = f"http://127.0.0.1:{PORT}"
API  = f"{BASE}/api"
TOKEN = None

# Icons served from Open WebUI's own static directory (full URL for API)
OPENAI_ICON  = f"http://127.0.0.1:{PORT}/static/openai-logo.svg"
MISTRAL_ICON = f"http://127.0.0.1:{PORT}/static/mistral-logo.svg"

MODELS = [
    {"id": "gpt-5",       "name": "GPT-5",       "icon": OPENAI_ICON,
     "desc": "OpenAI GPT-5 -- most capable reasoning model"},
    {"id": "gpt-5.1",     "name": "GPT-5.1",     "icon": OPENAI_ICON,
     "desc": "OpenAI GPT-5.1 -- latest iteration"},
    {"id": "gpt-5-mini",  "name": "GPT-5 Mini",  "icon": OPENAI_ICON,
     "desc": "OpenAI GPT-5 Mini -- fast and efficient"},
    {"id": "gpt-5-nano",  "name": "GPT-5 Nano",  "icon": OPENAI_ICON,
     "desc": "OpenAI GPT-5 Nano -- lightweight model"},
    {"id": "gpt-4.1",     "name": "GPT-4.1",     "icon": OPENAI_ICON,
     "desc": "OpenAI GPT-4.1 -- balanced performance"},
    {"id": "gpt-4o",      "name": "GPT-4o",      "icon": OPENAI_ICON,
     "desc": "OpenAI GPT-4o -- multimodal"},
    {"id": "gpt-oss-120b","name": "GPT-OSS 120B","icon": MISTRAL_ICON,
     "desc": "Open-source 120B parameter model"},
]


def api_request(method, path, data=None):
    """Make an authenticated API request to Open WebUI."""
    url = f"{API}{path}"
    body = json.dumps(data).encode() if data else None
    req = urllib.request.Request(url, data=body, method=method)
    req.add_header("Content-Type", "application/json")
    if TOKEN:
        req.add_header("Authorization", f"Bearer {TOKEN}")
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        try:
            return json.loads(e.read())
        except Exception:
            return {"status": e.code}
    except Exception as ex:
        print(f"  [init] Request failed: {ex}")
        return None


def authenticate():
    """Sign in as the default admin user (WEBUI_AUTH=false mode)."""
    global TOKEN
    data = {"email": "admin@localhost", "password": "admin"}
    result = api_request("POST", "/v1/auths/signin", data)
    if result and result.get("token"):
        TOKEN = result["token"]
        return True
    print(f"  [init] Auth failed: {result}")
    return False


def setup_model(m):
    """Create or update a workspace model with icon and description."""
    payload = {
        "id": m["id"],
        "name": m["name"],
        "base_model_id": m["id"],
        "meta": {
            "profile_image_url": m["icon"],
            "description": m["desc"],
        },
        "params": {},
        "is_active": True,
    }
    # Try create first
    result = api_request("POST", "/v1/models/create", payload)
    if result and result.get("id"):
        print(f"  [init] Created: {m['name']}")
        return
    # Already exists: update icon and description
    result = api_request("POST", "/v1/models/model/update", payload)
    if result and result.get("id"):
        print(f"  [init] Updated: {m['name']}")
        return
    print(f"  [init] Failed: {m['name']} ({result})")


def main():
    if not authenticate():
        print("[init] Skipping model setup (auth failed).")
        return
    print("[init] Configuring models...")
    for m in MODELS:
        setup_model(m)
    print("[init] Done.")


if __name__ == "__main__":
    main()
