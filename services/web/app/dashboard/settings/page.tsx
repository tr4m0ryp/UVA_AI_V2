"use client";

import { useState } from "react";

export default function SettingsPage() {
  const [cookie, setCookie] = useState("");
  const [saving, setSaving] = useState(false);
  const [status, setStatus] = useState<"idle" | "ok" | "error">("idle");

  async function save() {
    setSaving(true);
    setStatus("idle");
    try {
      const r = await fetch("/api/auth/session", {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ uva_session: cookie }),
      });
      setStatus(r.ok ? "ok" : "error");
    } catch {
      setStatus("error");
    } finally {
      setSaving(false);
    }
  }

  return (
    <div>
      <h1 style={{ fontSize: 20, fontWeight: 700, marginBottom: 20 }}>Settings</h1>

      <div className="card" style={{ maxWidth: 600 }}>
        <h2 style={{ fontSize: 15, fontWeight: 600, marginBottom: 8 }}>
          UvA Session Cookie
        </h2>
        <p style={{ color: "var(--text-muted)", fontSize: 13, marginBottom: 16 }}>
          Paste your UvA AI Chat session cookie here. This is used to authenticate
          requests to aichat.uva.nl on your behalf. To obtain it:
        </p>
        <ol style={{ color: "var(--text-muted)", fontSize: 13, paddingLeft: 20, marginBottom: 16, lineHeight: 2 }}>
          <li>Open aichat.uva.nl and sign in.</li>
          <li>Open DevTools (F12) and go to Application &rarr; Cookies.</li>
          <li>
            Copy all cookies for the site (or just the session token).
            The cookie string should look like{" "}
            <code>__Host-authjs.csrf-token=...; authjs.session-token=...</code>
          </li>
        </ol>
        <textarea
          style={{ width: "100%", height: 100, resize: "vertical", marginBottom: 12, fontFamily: "monospace" }}
          value={cookie}
          onChange={(e) => setCookie(e.target.value)}
          placeholder="Paste full cookie string here..."
        />
        {status === "ok" && (
          <p style={{ color: "var(--success)", fontSize: 13, marginBottom: 8 }}>
            Session cookie saved successfully.
          </p>
        )}
        {status === "error" && (
          <p style={{ color: "var(--danger)", fontSize: 13, marginBottom: 8 }}>
            Failed to save. Please try again.
          </p>
        )}
        <button
          className="btn btn-primary"
          onClick={save}
          disabled={saving || !cookie.trim()}
        >
          {saving ? "Saving..." : "Save Cookie"}
        </button>
      </div>
    </div>
  );
}
