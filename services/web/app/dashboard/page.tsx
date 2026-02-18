"use client";

import { useEffect, useState } from "react";
import Link from "next/link";

interface KeySummary {
  id: string;
  name: string;
  model: string;
  is_active: boolean;
  created_at: string;
}

interface ModelEntry {
  id: string;
  owned_by: string;
}

export default function DashboardPage() {
  const [keys, setKeys] = useState<KeySummary[]>([]);
  const [models, setModels] = useState<ModelEntry[]>([]);

  useEffect(() => {
    fetch("/api/keys")
      .then((r) => r.json())
      .then((data) => Array.isArray(data) && setKeys(data))
      .catch(() => {});

    fetch("/api/v1/models")
      .then((r) => r.json())
      .then((data) => Array.isArray(data?.data) && setModels(data.data))
      .catch(() => {});
  }, []);

  const activeKeys = keys.filter((k) => k.is_active).length;

  return (
    <div>
      <h1 style={{ fontSize: 20, fontWeight: 700, marginBottom: 20 }}>Overview</h1>

      <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 16, marginBottom: 24 }}>
        <div className="card">
          <div style={{ color: "var(--text-muted)", fontSize: 12, marginBottom: 4 }}>
            Total API Keys
          </div>
          <div style={{ fontSize: 28, fontWeight: 700 }}>{keys.length}</div>
        </div>
        <div className="card">
          <div style={{ color: "var(--text-muted)", fontSize: 12, marginBottom: 4 }}>
            Active Keys
          </div>
          <div style={{ fontSize: 28, fontWeight: 700, color: "var(--success)" }}>
            {activeKeys}
          </div>
        </div>
        <div className="card">
          <div style={{ color: "var(--text-muted)", fontSize: 12, marginBottom: 4 }}>
            Available Models
          </div>
          <div style={{ fontSize: 28, fontWeight: 700 }}>{models.length}</div>
        </div>
      </div>

      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 16 }}>
        <div className="card">
          <h2 style={{ fontSize: 14, fontWeight: 600, marginBottom: 12 }}>
            Available Models
          </h2>
          {models.length === 0 ? (
            <p style={{ color: "var(--text-muted)" }}>Loading...</p>
          ) : (
            <ul style={{ listStyle: "none" }}>
              {models.map((m) => (
                <li
                  key={m.id}
                  style={{
                    padding: "4px 0",
                    borderBottom: "1px solid var(--border)",
                    color: "var(--foreground)",
                    fontSize: 13,
                  }}
                >
                  {m.id}
                </li>
              ))}
            </ul>
          )}
        </div>

        <div className="card">
          <div style={{ display: "flex", justifyContent: "space-between", marginBottom: 12 }}>
            <h2 style={{ fontSize: 14, fontWeight: 600 }}>Recent Keys</h2>
            <Link href="/dashboard/keys" style={{ fontSize: 12 }}>
              Manage
            </Link>
          </div>
          {keys.length === 0 ? (
            <p style={{ color: "var(--text-muted)" }}>
              No API keys yet.{" "}
              <Link href="/dashboard/keys">Create one</Link>.
            </p>
          ) : (
            <ul style={{ listStyle: "none" }}>
              {keys.slice(0, 5).map((k) => (
                <li
                  key={k.id}
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    padding: "4px 0",
                    borderBottom: "1px solid var(--border)",
                    fontSize: 13,
                  }}
                >
                  <span>{k.name || "(unnamed)"}</span>
                  <span className={`badge ${k.is_active ? "badge-green" : "badge-red"}`}>
                    {k.is_active ? "active" : "inactive"}
                  </span>
                </li>
              ))}
            </ul>
          )}
        </div>
      </div>
    </div>
  );
}
