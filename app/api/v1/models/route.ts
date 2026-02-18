import { NextResponse } from "next/server";
import { listModels } from "@/lib/uva/models";

export async function GET() {
  return NextResponse.json(listModels());
}
