import { auth } from "@/auth";
import { createServerClient } from "./supabase";
import { NextResponse } from "next/server";

export async function getServerUser() {
  const session = await auth();
  if (!session?.user?.email) return null;
  return session.user;
}

// Returns the Supabase user row, creating it if needed.
export async function getOrCreateUser(email: string, name?: string | null) {
  const db = createServerClient();

  const { data: existing } = await db
    .from("users")
    .select("*")
    .eq("email", email)
    .single();

  if (existing) {
    // Update last_login
    await db
      .from("users")
      .update({ last_login: new Date().toISOString(), name: name ?? existing.name })
      .eq("id", existing.id);
    return existing;
  }

  const { data: created, error } = await db
    .from("users")
    .insert({ email, name: name ?? null, uva_session: "" })
    .select()
    .single();

  if (error || !created) {
    throw new Error(`Failed to create user: ${error?.message}`);
  }

  return created;
}

// Middleware helper: require auth on API routes.
// Returns user row or a 401 NextResponse.
export async function requireApiAuth(): Promise<
  { type: "user"; user: Record<string, unknown> } | { type: "error"; response: NextResponse }
> {
  const session = await auth();
  if (!session?.user?.email) {
    return {
      type: "error",
      response: NextResponse.json({ error: "Unauthorized" }, { status: 401 }),
    };
  }

  try {
    const user = await getOrCreateUser(
      session.user.email,
      session.user.name ?? undefined
    );
    return { type: "user", user };
  } catch (err) {
    return {
      type: "error",
      response: NextResponse.json(
        { error: "Failed to load user" },
        { status: 500 }
      ),
    };
  }
}
