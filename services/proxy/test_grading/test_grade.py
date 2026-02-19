#!/usr/bin/env python3
"""End-to-end grading test -- mimics grading-api.js exactly."""
import json, os, sys, urllib.request

DIR = os.path.dirname(os.path.abspath(__file__))
BASE = "http://localhost:8787"
KEY = "uva-sk-7d0873b04d8f907bbc5867368f87b96d8e3c9dfac87ed16db91559c1923c1798"
MODEL = "gpt-5.1"

def read(name):
    with open(os.path.join(DIR, name)) as f:
        return f.read()

assignment = read("assignment.txt")
rubric = read("rubric.txt")
example = read("example_answer.txt")

system_prompt = f"""You are a strict academic grader. Grade the following student submission based on the provided rubric.

ASSIGNMENT:
{assignment}

RUBRIC:
{rubric}

EXAMPLE ANSWER (for reference):
{example}

You MUST respond with ONLY a valid JSON object (no markdown, no text outside JSON) in this exact format:
{{
  "criteria": [
    {{ "name": "<criterion name>", "score": <number>, "maxScore": <number>, "feedback": "<specific feedback>" }}
  ],
  "overallFeedback": "<2-3 sentence summary>"
}}

Rules:
- Extract criteria names and max scores from the rubric
- Score each criterion from 0 to its maxScore
- Be rigorous and fair
- Provide specific, actionable feedback
- Do NOT output any text outside the JSON object"""

# 3 test submissions: excellent, average, bad
tests = [
    ("emma_de_vries_14000000.py", "Emma de Vries", "14000000"),
    ("ethan_peters_14000370.py", "Ethan Peters", "14000370"),
    ("henry_prins_14001120.py", "Henry Prins", "14001120"),
]

for fname, name, sid in tests:
    content = read(f"submissions/{fname}")
    user_msg = f"Student: {name} (ID: {sid})\n\n--- BEGIN SUBMISSION ---\n{content}\n--- END SUBMISSION ---"

    payload = json.dumps({
        "model": MODEL,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_msg},
        ],
        "temperature": 0.2,
    }).encode()

    req = urllib.request.Request(
        f"{BASE}/v1/chat/completions",
        data=payload,
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {KEY}",
        },
    )

    print(f"=== {name} ({sid}) -- {fname} ===")
    try:
        with urllib.request.urlopen(req, timeout=90) as resp:
            raw = resp.read().decode()
            r = json.loads(raw)
            if "error" in r:
                print(f"  API ERROR: {r['error']}")
                print(f"  STATUS: FAIL")
                print()
                continue
            full = r["choices"][0]["message"]["content"]

        # Strip markdown fences
        text = full.strip()
        if text.startswith("```"):
            lines = text.split("\n")
            text = "\n".join(lines[1:])
            if text.endswith("```"):
                text = text[:-3].strip()

        parsed = json.loads(text)
        total = sum(c["score"] for c in parsed["criteria"])
        mx = sum(c["maxScore"] for c in parsed["criteria"])
        pct = round(total / mx * 100) if mx else 0
        print(f"  Score: {total}/{mx} ({pct}%)")
        for c in parsed["criteria"]:
            print(f"    {c['name']}: {c['score']}/{c['maxScore']}")
        fb = parsed["overallFeedback"]
        print(f"  Feedback: {fb[:150]}...")
        print(f"  STATUS: OK")
    except json.JSONDecodeError:
        print(f"  JSON PARSE FAIL -- raw output (first 300 chars):")
        print(f"  {full[:300]}")
        print(f"  STATUS: FAIL (model did not return valid JSON)")
    except Exception as e:
        print(f"  ERROR: {e}")
        print(f"  STATUS: FAIL")
    print()
