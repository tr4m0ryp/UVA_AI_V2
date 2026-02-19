#!/usr/bin/env python3
"""Batch test: grade all 30 submissions, report summary stats."""
import json, os, sys, time, urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed

DIR = os.path.dirname(os.path.abspath(__file__))
BASE = "http://localhost:8787"
KEY = "uva-sk-7d0873b04d8f907bbc5867368f87b96d8e3c9dfac87ed16db91559c1923c1798"
MODEL = "gpt-5-nano"  # fast model for batch testing

def read(name):
    with open(os.path.join(DIR, name)) as f:
        return f.read()

assignment = read("assignment.txt")
rubric = read("rubric.txt")
example = read("example_answer.txt")

system_prompt = (
    "You are a strict academic grader. Grade the student submission "
    "according to the assignment and rubric below. "
    "Respond ONLY with valid JSON, no markdown fences, no other text.\n\n"
    f"ASSIGNMENT:\n{assignment}\n\n"
    f"RUBRIC:\n{rubric}\n\n"
    f"EXAMPLE ANSWER:\n{example}\n\n"
    'JSON format: {"criteria":[{"name":"...","score":<n>,"maxScore":<n>,'
    '"feedback":"..."}],"overallFeedback":"..."}\n\n'
    "Rules: extract criteria from rubric, score 0 to maxScore, be rigorous."
)

# Load manifest
submissions = []
sub_dir = os.path.join(DIR, "submissions")
for fname in sorted(os.listdir(sub_dir)):
    if not fname.endswith(".py"):
        continue
    parts = fname.replace(".py", "").rsplit("_", 1)
    sid = parts[-1] if len(parts) > 1 else "unknown"
    name = fname.replace(".py", "").rsplit("_", 1)[0].replace("_", " ").title()
    submissions.append((fname, name, sid))

print(f"Grading {len(submissions)} submissions with {MODEL}...\n")

def grade_one(fname, name, sid):
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
        headers={"Content-Type": "application/json",
                 "Authorization": f"Bearer {KEY}"},
    )

    t0 = time.time()
    with urllib.request.urlopen(req, timeout=120) as resp:
        raw = resp.read().decode()

    elapsed = time.time() - t0
    r = json.loads(raw)
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
    return {
        "name": name, "sid": sid, "fname": fname,
        "score": total, "max": mx, "pct": pct,
        "time": round(elapsed, 1),
        "feedback": parsed["overallFeedback"][:80],
    }

# Grade sequentially (proxy handles one upstream request at a time)
results = []
errors = []
for i, (fname, name, sid) in enumerate(submissions):
    try:
        r = grade_one(fname, name, sid)
        results.append(r)
        bar = "#" * (r["pct"] // 5) + "." * (20 - r["pct"] // 5)
        print(f"  [{i+1:2d}/30] {r['name']:<25s} {r['score']:3d}/{r['max']} "
              f"({r['pct']:3d}%) [{bar}] {r['time']}s")
    except Exception as e:
        errors.append((fname, name, str(e)[:80]))
        print(f"  [{i+1:2d}/30] {name:<25s} FAIL: {str(e)[:60]}")

# Summary
print(f"\n{'='*60}")
print(f"Results: {len(results)} graded, {len(errors)} errors\n")

if results:
    pcts = [r["pct"] for r in results]
    print(f"  Average: {sum(pcts)/len(pcts):.1f}%")
    print(f"  Median:  {sorted(pcts)[len(pcts)//2]}%")
    print(f"  Range:   {min(pcts)}% - {max(pcts)}%")
    print(f"  Pass (>=55%): {sum(1 for p in pcts if p >= 55)}/{len(pcts)}")

    # Distribution
    bins = {"90-100": 0, "80-89": 0, "70-79": 0, "60-69": 0,
            "50-59": 0, "40-49": 0, "0-39": 0}
    for p in pcts:
        if p >= 90: bins["90-100"] += 1
        elif p >= 80: bins["80-89"] += 1
        elif p >= 70: bins["70-79"] += 1
        elif p >= 60: bins["60-69"] += 1
        elif p >= 50: bins["50-59"] += 1
        elif p >= 40: bins["40-49"] += 1
        else: bins["0-39"] += 1
    print("\n  Distribution:")
    for k, v in bins.items():
        bar = "#" * v
        print(f"    {k:>6s}: {bar} ({v})")

if errors:
    print(f"\n  Errors:")
    for fname, name, err in errors:
        print(f"    {name}: {err}")
