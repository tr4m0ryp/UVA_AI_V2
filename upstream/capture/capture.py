"""
mitmproxy addon to capture and log UvA AI Chat API traffic.

Usage:
  mitmdump -s capture.py --listen-port 8080 --set stream_large_bodies=0

Then configure Firefox:
  Settings -> Network -> Manual Proxy -> HTTP: localhost, Port: 8080
  Also set HTTPS proxy to localhost:8080
  Visit http://mitm.it to install the CA certificate (required for HTTPS)

Navigate to aichat.uva.nl, log in, send chat messages.
Press Ctrl+C when done. Results saved to captured_traffic.json.
"""
import json
import time
from mitmproxy import http, ctx

captured = []

class UvACapture:
    def response(self, flow: http.HTTPFlow):
        if "aichat.uva.nl" not in (flow.request.pretty_host or ""):
            return

        url = flow.request.pretty_url
        method = flow.request.method

        # Skip static assets
        if any(url.endswith(ext) for ext in
               ['.js', '.css', '.png', '.ico', '.svg', '.woff2', '.map']):
            return
        if '/_next/static/' in url and '/chunks/' not in url:
            return

        entry = {
            "timestamp": time.strftime("%H:%M:%S"),
            "method": method,
            "url": url,
            "status": flow.response.status_code if flow.response else None,
            "request_headers": dict(flow.request.headers),
            "request_content_type": flow.request.headers.get("content-type", ""),
        }

        # Capture request body
        if flow.request.content:
            try:
                body = flow.request.content.decode("utf-8", errors="replace")
                # Try to parse as JSON for pretty display
                try:
                    entry["request_body"] = json.loads(body)
                except (json.JSONDecodeError, ValueError):
                    entry["request_body_raw"] = body[:4096]
            except Exception:
                entry["request_body"] = "(binary)"

        # Capture response
        if flow.response:
            entry["response_headers"] = dict(flow.response.headers)
            entry["response_content_type"] = flow.response.headers.get(
                "content-type", "")

            if flow.response.content:
                try:
                    resp = flow.response.content.decode("utf-8", errors="replace")
                    try:
                        entry["response_body"] = json.loads(resp)
                    except (json.JSONDecodeError, ValueError):
                        # Truncate large responses but keep enough to analyze
                        entry["response_body_raw"] = resp[:8192]
                except Exception:
                    entry["response_body"] = "(binary)"

        captured.append(entry)

        # Highlight important endpoints
        marker = ""
        if "/api/v1/chat" in url:
            marker = " *** CHAT API ***"
        elif "Next-Action" in str(flow.request.headers):
            action_id = flow.request.headers.get("Next-Action", "unknown")
            marker = f" [ServerAction: {action_id[:20]}...]"
        elif "/api/auth" in url:
            marker = " [AUTH]"

        status = flow.response.status_code if flow.response else "?"
        ctx.log.info(f"[UvA] {method} {url} -> {status}{marker}")

    def done(self):
        outfile = "captured_traffic.json"
        with open(outfile, "w") as f:
            json.dump(captured, f, indent=2, default=str)
        ctx.log.info(f"Saved {len(captured)} requests to {outfile}")

        # Also produce a summary
        summary_file = "capture_summary.txt"
        with open(summary_file, "w") as f:
            f.write("UvA AI Chat Traffic Capture Summary\n")
            f.write("=" * 60 + "\n\n")

            chat_reqs = [e for e in captured if "/api/v1/chat" in e.get("url", "")]
            action_reqs = [e for e in captured
                          if "Next-Action" in str(e.get("request_headers", {}))]
            auth_reqs = [e for e in captured if "/api/auth" in e.get("url", "")]

            f.write(f"Total captured: {len(captured)} requests\n")
            f.write(f"Chat API calls: {len(chat_reqs)}\n")
            f.write(f"Server actions: {len(action_reqs)}\n")
            f.write(f"Auth requests:  {len(auth_reqs)}\n\n")

            if chat_reqs:
                f.write("CHAT API REQUESTS\n")
                f.write("-" * 40 + "\n")
                for i, req in enumerate(chat_reqs):
                    f.write(f"\n--- Chat Request #{i+1} ---\n")
                    f.write(f"URL: {req.get('url')}\n")
                    f.write(f"Status: {req.get('status')}\n")
                    f.write(f"Content-Type: {req.get('request_content_type')}\n")
                    if "request_body" in req:
                        f.write(f"Request Body:\n{json.dumps(req['request_body'], indent=2)}\n")
                    elif "request_body_raw" in req:
                        f.write(f"Request Body (raw):\n{req['request_body_raw'][:2000]}\n")
                    f.write(f"Response Content-Type: {req.get('response_content_type')}\n")
                    if "response_body" in req:
                        resp_str = json.dumps(req["response_body"], indent=2)
                        f.write(f"Response Body:\n{resp_str[:2000]}\n")
                    elif "response_body_raw" in req:
                        f.write(f"Response Body (raw):\n{req['response_body_raw'][:2000]}\n")

            if action_reqs:
                f.write("\n\nSERVER ACTION REQUESTS\n")
                f.write("-" * 40 + "\n")
                for i, req in enumerate(action_reqs):
                    hdrs = req.get("request_headers", {})
                    action_id = hdrs.get("Next-Action", hdrs.get("next-action", "?"))
                    f.write(f"\n--- Action #{i+1}: {action_id} ---\n")
                    f.write(f"Status: {req.get('status')}\n")
                    if "request_body" in req:
                        f.write(f"Body: {json.dumps(req['request_body'], indent=2)[:1000]}\n")
                    elif "request_body_raw" in req:
                        f.write(f"Body (raw): {req['request_body_raw'][:1000]}\n")
                    if "response_body_raw" in req:
                        f.write(f"Response: {req['response_body_raw'][:1000]}\n")

            if auth_reqs:
                f.write("\n\nAUTH REQUESTS\n")
                f.write("-" * 40 + "\n")
                for req in auth_reqs:
                    f.write(f"  {req.get('method')} {req.get('url')} -> {req.get('status')}\n")

            # Extract cookie names from any request
            all_cookies = set()
            for req in captured:
                cookie = req.get("request_headers", {}).get("cookie",
                         req.get("request_headers", {}).get("Cookie", ""))
                if cookie:
                    for part in cookie.split(";"):
                        name = part.strip().split("=")[0]
                        if name:
                            all_cookies.add(name)
            if all_cookies:
                f.write(f"\n\nCOOKIE NAMES OBSERVED\n")
                f.write("-" * 40 + "\n")
                for name in sorted(all_cookies):
                    f.write(f"  {name}\n")

        ctx.log.info(f"Saved summary to {summary_file}")


addons = [UvACapture()]
