"use client";

import { useEffect, useState } from "react";

interface ApiKey {
  id: string;
  key_prefix: string;
  name: string;
  model: string;
  temperature: number;
  max_tokens: number;
  system_prompt: string;
  reasoning_effort: string;
  is_active: boolean;
  created_at: string;
  last_used: string | null;
}

const MODELS = [
  "gpt-5", "gpt-5.1", "gpt-5-mini", "gpt-5-nano",
  "gpt-4.1", "gpt-4o", "gpt-oss-120b",
];

const DEFAULT_FORM = {
  name: "",
  model: "gpt-4o",
  temperature: 0.5,
  max_tokens: 4096,
  system_prompt: "",
  reasoning_effort: "medium",
};

export default function KeysPage() {
  const [keys, setKeys] = useState<ApiKey[]>([]);
  const [showCreate, setShowCreate] = useState(false);
  const [form, setForm] = useState(DEFAULT_FORM);
  const [creating, setCreating] = useState(false);
  const [newKeyRaw, setNewKeyRaw] = useState<string | null>(null);
  const [editId, setEditId] = useState<string | null>(null);
  const [editForm, setEditForm] = useState(DEFAULT_FORM);

  async function loadKeys() {
    const r = await fetch("/api/keys");
    if (r.ok) setKeys(await r.json());
  }

  useEffect(() => {
    loadKeys();
  }, []);

  async function createKey() {
    setCreating(true);
    const r = await fetch("/api/keys", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(form),
    });
    setCreating(false);
    if (r.ok) {
      const data = await r.json();
      setNewKeyRaw(data.raw_key);
      setForm(DEFAULT_FORM);
      setShowCreate(false);
      await loadKeys();
    }
  }

  async function deleteKey(id: string) {
    if (!confirm("Delete this API key?")) return;
    await fetch(`/api/keys/${id}`, { method: "DELETE" });
    await loadKeys();
  }

  async function toggleKey(id: string) {
    await fetch(`/api/keys/${id}/toggle`, { method: "POST" });
    await loadKeys();
  }

  async function saveEdit() {
    if (!editId) return;
    await fetch(`/api/keys/${editId}`, {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(editForm),
    });
    setEditId(null);
    await loadKeys();
  }

  function startEdit(k: ApiKey) {
    setEditId(k.id);
    setEditForm({
      name: k.name,
      model: k.model,
      temperature: k.temperature,
      max_tokens: k.max_tokens,
      system_prompt: k.system_prompt,
      reasoning_effort: k.reasoning_effort,
    });
  }

  return (
    <div>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 20 }}>
        <h1 style={{ fontSize: 20, fontWeight: 700 }}>API Keys</h1>
        <button className="btn btn-primary" onClick={() => setShowCreate(true)}>
          + New Key
        </button>
      </div>

      {newKeyRaw && (
        <div className="card" style={{ marginBottom: 16, borderColor: "var(--success)" }}>
          <p style={{ fontWeight: 600, marginBottom: 6 }}>
            Key created. Copy it now - it will not be shown again.
          </p>
          <code
            style={{
              display: "block",
              background: "var(--background)",
              padding: "8px 12px",
              borderRadius: 4,
              wordBreak: "break-all",
              color: "var(--success)",
            }}
          >
            {newKeyRaw}
          </code>
          <button
            className="btn btn-secondary"
            style={{ marginTop: 10 }}
            onClick={() => setNewKeyRaw(null)}
          >
            Dismiss
          </button>
        </div>
      )}

      {showCreate && (
        <div className="card" style={{ marginBottom: 20 }}>
          <h2 style={{ fontSize: 15, fontWeight: 600, marginBottom: 14 }}>Create API Key</h2>
          <FormFields form={form} onChange={setForm} />
          <div style={{ display: "flex", gap: 8, marginTop: 14 }}>
            <button className="btn btn-primary" onClick={createKey} disabled={creating}>
              {creating ? "Creating..." : "Create"}
            </button>
            <button className="btn btn-secondary" onClick={() => setShowCreate(false)}>
              Cancel
            </button>
          </div>
        </div>
      )}

      {editId && (
        <div className="card" style={{ marginBottom: 20 }}>
          <h2 style={{ fontSize: 15, fontWeight: 600, marginBottom: 14 }}>Edit Key</h2>
          <FormFields form={editForm} onChange={setEditForm} />
          <div style={{ display: "flex", gap: 8, marginTop: 14 }}>
            <button className="btn btn-primary" onClick={saveEdit}>Save</button>
            <button className="btn btn-secondary" onClick={() => setEditId(null)}>Cancel</button>
          </div>
        </div>
      )}

      {keys.length === 0 ? (
        <p style={{ color: "var(--text-muted)" }}>No API keys yet.</p>
      ) : (
        <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
          {keys.map((k) => (
            <div
              key={k.id}
              className="card"
              style={{
                display: "flex",
                justifyContent: "space-between",
                alignItems: "center",
                opacity: k.is_active ? 1 : 0.55,
              }}
            >
              <div>
                <div style={{ fontWeight: 600 }}>{k.name || "(unnamed)"}</div>
                <div style={{ color: "var(--text-muted)", fontSize: 12, marginTop: 2 }}>
                  {k.key_prefix}... &bull; {k.model} &bull; temp {k.temperature}
                  {k.last_used && ` &bull; last used ${k.last_used.slice(0, 10)}`}
                </div>
              </div>
              <div style={{ display: "flex", gap: 8, alignItems: "center" }}>
                <span className={`badge ${k.is_active ? "badge-green" : "badge-red"}`}>
                  {k.is_active ? "active" : "inactive"}
                </span>
                <button className="btn btn-secondary" style={{ fontSize: 12 }} onClick={() => startEdit(k)}>
                  Edit
                </button>
                <button className="btn btn-secondary" style={{ fontSize: 12 }} onClick={() => toggleKey(k.id)}>
                  {k.is_active ? "Disable" : "Enable"}
                </button>
                <button className="btn btn-danger" style={{ fontSize: 12 }} onClick={() => deleteKey(k.id)}>
                  Delete
                </button>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

function FormFields({
  form,
  onChange,
}: {
  form: typeof DEFAULT_FORM;
  onChange: (f: typeof DEFAULT_FORM) => void;
}) {
  function set(field: string, value: string | number) {
    onChange({ ...form, [field]: value });
  }

  return (
    <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 12 }}>
      <div>
        <label style={{ display: "block", fontSize: 12, marginBottom: 4 }}>Name</label>
        <input
          style={{ width: "100%" }}
          value={form.name}
          onChange={(e) => set("name", e.target.value)}
          placeholder="My key"
        />
      </div>
      <div>
        <label style={{ display: "block", fontSize: 12, marginBottom: 4 }}>Model</label>
        <select style={{ width: "100%" }} value={form.model} onChange={(e) => set("model", e.target.value)}>
          {MODELS.map((m) => (
            <option key={m} value={m}>{m}</option>
          ))}
        </select>
      </div>
      <div>
        <label style={{ display: "block", fontSize: 12, marginBottom: 4 }}>Temperature</label>
        <input
          type="number"
          style={{ width: "100%" }}
          min={0}
          max={2}
          step={0.1}
          value={form.temperature}
          onChange={(e) => set("temperature", parseFloat(e.target.value))}
        />
      </div>
      <div>
        <label style={{ display: "block", fontSize: 12, marginBottom: 4 }}>Max Tokens</label>
        <input
          type="number"
          style={{ width: "100%" }}
          min={256}
          max={32768}
          value={form.max_tokens}
          onChange={(e) => set("max_tokens", parseInt(e.target.value))}
        />
      </div>
      <div>
        <label style={{ display: "block", fontSize: 12, marginBottom: 4 }}>Reasoning Effort</label>
        <select style={{ width: "100%" }} value={form.reasoning_effort} onChange={(e) => set("reasoning_effort", e.target.value)}>
          <option value="low">low</option>
          <option value="medium">medium</option>
          <option value="high">high</option>
        </select>
      </div>
      <div style={{ gridColumn: "1 / -1" }}>
        <label style={{ display: "block", fontSize: 12, marginBottom: 4 }}>System Prompt</label>
        <textarea
          style={{ width: "100%", height: 80, resize: "vertical" }}
          value={form.system_prompt}
          onChange={(e) => set("system_prompt", e.target.value)}
          placeholder="Optional system prompt..."
        />
      </div>
    </div>
  );
}
