"use client";

import { signIn } from "next-auth/react";
import { useState } from "react";

export default function LoginPage() {
  const [loading, setLoading] = useState(false);

  async function handleLogin() {
    setLoading(true);
    await signIn("microsoft-entra-id", { callbackUrl: "/dashboard" });
  }

  return (
    <div
      style={{
        minHeight: "100vh",
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
      }}
    >
      <div className="card" style={{ width: 360, textAlign: "center" }}>
        <h1 style={{ fontSize: 20, fontWeight: 700, marginBottom: 8 }}>
          UvA AI Proxy
        </h1>
        <p style={{ color: "var(--text-muted)", marginBottom: 24 }}>
          Sign in with your UvA account to manage API keys.
        </p>
        <button
          className="btn btn-primary"
          style={{ width: "100%", justifyContent: "center", padding: "10px 0" }}
          onClick={handleLogin}
          disabled={loading}
        >
          {loading ? "Redirecting..." : "Sign in with UvA (Azure AD)"}
        </button>
        <p
          style={{
            marginTop: 16,
            fontSize: 12,
            color: "var(--text-muted)",
          }}
        >
          UvA, HvA, and Npuls accounts supported
        </p>
      </div>
    </div>
  );
}
