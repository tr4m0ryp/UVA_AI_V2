"use client";

import { useEffect, useState } from "react";

interface KeySummary {
  id: string;
  name: string;
  model: string;
  key_prefix: string;
}

interface UsageData {
  total_requests: number;
  total_input_chars: number;
  total_output_chars: number;
  success_count: number;
  avg_latency_ms: number;
  daily: Record<string, { requests: number; output_chars: number }>;
}

export default function UsagePage() {
  const [keys, setKeys] = useState<KeySummary[]>([]);
  const [selectedKey, setSelectedKey] = useState<string>("");
  const [usage, setUsage] = useState<UsageData | null>(null);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    fetch("/api/keys")
      .then((r) => r.json())
      .then((data) => {
        if (Array.isArray(data)) {
          setKeys(data);
          if (data.length > 0) setSelectedKey(data[0].id);
        }
      })
      .catch(() => {});
  }, []);

  useEffect(() => {
    if (!selectedKey) return;
    setLoading(true);
    fetch(`/api/keys/${selectedKey}/usage`)
      .then((r) => r.json())
      .then((data) => setUsage(data))
      .catch(() => {})
      .finally(() => setLoading(false));
  }, [selectedKey]);

  const selectedKeyInfo = keys.find((k) => k.id === selectedKey);

  const dailyEntries = usage
    ? Object.entries(usage.daily)
        .sort(([a], [b]) => a.localeCompare(b))
        .slice(-14)
    : [];

  return (
    <div>
      <h1 style={{ fontSize: 20, fontWeight: 700, marginBottom: 20 }}>Usage</h1>

      {keys.length === 0 ? (
        <p style={{ color: "var(--text-muted)" }}>No API keys found.</p>
      ) : (
        <>
          <div style={{ marginBottom: 20 }}>
            <label style={{ fontSize: 12, marginRight: 8 }}>API Key:</label>
            <select
              value={selectedKey}
              onChange={(e) => setSelectedKey(e.target.value)}
            >
              {keys.map((k) => (
                <option key={k.id} value={k.id}>
                  {k.name || "(unnamed)"} — {k.key_prefix}...
                </option>
              ))}
            </select>
          </div>

          {loading ? (
            <p style={{ color: "var(--text-muted)" }}>Loading...</p>
          ) : usage ? (
            <>
              <div
                style={{
                  display: "grid",
                  gridTemplateColumns: "repeat(4, 1fr)",
                  gap: 16,
                  marginBottom: 24,
                }}
              >
                <StatCard label="Total Requests" value={usage.total_requests} />
                <StatCard label="Success Rate" value={
                  usage.total_requests > 0
                    ? `${Math.round((usage.success_count / usage.total_requests) * 100)}%`
                    : "N/A"
                } />
                <StatCard label="Avg Latency" value={`${usage.avg_latency_ms}ms`} />
                <StatCard
                  label="Output Chars"
                  value={usage.total_output_chars.toLocaleString()}
                />
              </div>

              <div className="card">
                <h2 style={{ fontSize: 14, fontWeight: 600, marginBottom: 16 }}>
                  Daily Requests (last 14 days) — {selectedKeyInfo?.name || "key"}
                </h2>
                {dailyEntries.length === 0 ? (
                  <p style={{ color: "var(--text-muted)" }}>No data yet.</p>
                ) : (
                  <div style={{ overflowX: "auto" }}>
                    <table style={{ width: "100%", borderCollapse: "collapse", fontSize: 13 }}>
                      <thead>
                        <tr style={{ borderBottom: "1px solid var(--border)" }}>
                          <th style={{ textAlign: "left", padding: "4px 8px" }}>Date</th>
                          <th style={{ textAlign: "right", padding: "4px 8px" }}>Requests</th>
                          <th style={{ textAlign: "right", padding: "4px 8px" }}>Output Chars</th>
                        </tr>
                      </thead>
                      <tbody>
                        {dailyEntries.map(([date, stats]) => (
                          <tr key={date} style={{ borderBottom: "1px solid var(--border)" }}>
                            <td style={{ padding: "4px 8px" }}>{date}</td>
                            <td style={{ textAlign: "right", padding: "4px 8px" }}>
                              {stats.requests}
                            </td>
                            <td style={{ textAlign: "right", padding: "4px 8px" }}>
                              {stats.output_chars.toLocaleString()}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )}
              </div>
            </>
          ) : null}
        </>
      )}
    </div>
  );
}

function StatCard({ label, value }: { label: string; value: string | number }) {
  return (
    <div className="card">
      <div style={{ color: "var(--text-muted)", fontSize: 12, marginBottom: 4 }}>
        {label}
      </div>
      <div style={{ fontSize: 24, fontWeight: 700 }}>{value}</div>
    </div>
  );
}
