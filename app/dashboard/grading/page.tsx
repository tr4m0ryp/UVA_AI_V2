"use client";

import { useEffect, useState } from "react";

interface GradingSession {
  id: string;
  title: string;
  submission_count: number;
  created_at: string;
  updated_at: string;
}

export default function GradingPage() {
  const [sessions, setSessions] = useState<GradingSession[]>([]);
  const [showCreate, setShowCreate] = useState(false);
  const [title, setTitle] = useState("");
  const [creating, setCreating] = useState(false);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [detail, setDetail] = useState<Record<string, unknown> | null>(null);

  async function loadSessions() {
    const r = await fetch("/api/grading");
    if (r.ok) setSessions(await r.json());
  }

  useEffect(() => {
    loadSessions();
  }, []);

  async function createSession() {
    if (!title.trim()) return;
    setCreating(true);
    const r = await fetch("/api/grading", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ title }),
    });
    setCreating(false);
    if (r.ok) {
      setTitle("");
      setShowCreate(false);
      await loadSessions();
    }
  }

  async function deleteSession(id: string) {
    if (!confirm("Delete this grading session?")) return;
    await fetch(`/api/grading/${id}`, { method: "DELETE" });
    if (selectedId === id) {
      setSelectedId(null);
      setDetail(null);
    }
    await loadSessions();
  }

  async function viewSession(id: string) {
    setSelectedId(id);
    const r = await fetch(`/api/grading/${id}`);
    if (r.ok) setDetail(await r.json());
  }

  return (
    <div>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 20 }}>
        <h1 style={{ fontSize: 20, fontWeight: 700 }}>Grading Sessions</h1>
        <button className="btn btn-primary" onClick={() => setShowCreate(true)}>
          + New Session
        </button>
      </div>

      {showCreate && (
        <div className="card" style={{ marginBottom: 20 }}>
          <h2 style={{ fontSize: 15, fontWeight: 600, marginBottom: 10 }}>Create Session</h2>
          <input
            style={{ width: "100%", marginBottom: 10 }}
            value={title}
            onChange={(e) => setTitle(e.target.value)}
            placeholder="Session title..."
            onKeyDown={(e) => e.key === "Enter" && createSession()}
          />
          <div style={{ display: "flex", gap: 8 }}>
            <button className="btn btn-primary" onClick={createSession} disabled={creating}>
              {creating ? "Creating..." : "Create"}
            </button>
            <button className="btn btn-secondary" onClick={() => setShowCreate(false)}>
              Cancel
            </button>
          </div>
        </div>
      )}

      <div style={{ display: "grid", gridTemplateColumns: selectedId ? "1fr 1fr" : "1fr", gap: 16 }}>
        <div>
          {sessions.length === 0 ? (
            <p style={{ color: "var(--text-muted)" }}>No grading sessions yet.</p>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
              {sessions.map((s) => (
                <div
                  key={s.id}
                  className="card"
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center",
                    borderColor: selectedId === s.id ? "var(--accent)" : "var(--border)",
                  }}
                >
                  <div>
                    <div style={{ fontWeight: 600 }}>{s.title}</div>
                    <div style={{ color: "var(--text-muted)", fontSize: 12 }}>
                      {s.submission_count} submissions &bull; {s.created_at.slice(0, 10)}
                    </div>
                  </div>
                  <div style={{ display: "flex", gap: 8 }}>
                    <button className="btn btn-secondary" style={{ fontSize: 12 }} onClick={() => viewSession(s.id)}>
                      View
                    </button>
                    <button className="btn btn-danger" style={{ fontSize: 12 }} onClick={() => deleteSession(s.id)}>
                      Delete
                    </button>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {selectedId && detail && (
          <div className="card">
            <h2 style={{ fontSize: 14, fontWeight: 600, marginBottom: 12 }}>
              {(detail.title as string) || "Session Detail"}
            </h2>
            <div style={{ fontSize: 12, color: "var(--text-muted)", marginBottom: 12 }}>
              Created: {(detail.created_at as string)?.slice(0, 10)} &bull;{" "}
              Updated: {(detail.updated_at as string)?.slice(0, 10)}
            </div>
            <div style={{ fontSize: 12, marginBottom: 8 }}>
              Submissions: {detail.submission_count as number}
            </div>
            <div>
              <div style={{ fontSize: 12, color: "var(--text-muted)", marginBottom: 4 }}>
                Results JSON:
              </div>
              <pre
                style={{
                  background: "var(--background)",
                  padding: 10,
                  borderRadius: 4,
                  fontSize: 11,
                  overflowX: "auto",
                  maxHeight: 300,
                }}
              >
                {JSON.stringify(detail.results_json, null, 2)}
              </pre>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
