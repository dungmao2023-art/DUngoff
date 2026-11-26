#!/usr/bin/env python3
"""Calculate Dung Office scope readiness and remaining work from the 48 acceptance groups."""
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path

SCORES = {"scoped": 0, "partial": 0.25, "integrated": 0.60, "verified": 0.85, "accepted": 1}
ROOT = Path(__file__).resolve().parents[1]

def load(path):
    with path.open(encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))
    seen = set()
    for row in rows:
        key = row["id"]
        if not key or key in seen:
            raise ValueError(f"Duplicate or empty ID: {key}")
        seen.add(key)
        if row["priority"] not in ("P0", "P1") or row["stage"] not in SCORES:
            raise ValueError(f"Invalid priority/stage: {key}")
        for field in ("weight", "remaining_low_hours", "remaining_high_hours"):
            row[field] = int(row[field])
        if row["weight"] <= 0 or not 0 <= row["remaining_low_hours"] <= row["remaining_high_hours"]:
            raise ValueError(f"Invalid weight/hours: {key}")
        if row["stage"] == "accepted" and (row["remaining_high_hours"] or not row["evidence_or_blocker"].strip()):
            raise ValueError(f"Accepted item requires zero remaining hours and evidence: {key}")
    if not rows:
        raise ValueError("Empty checklist")
    return rows

def summarize(rows):
    total = sum(r["weight"] for r in rows)
    score = sum(r["weight"] * SCORES[r["stage"]] for r in rows)
    return (len(rows), sum(r["stage"] == "accepted" for r in rows), 100 * score / total,
            sum(r["remaining_low_hours"] for r in rows), sum(r["remaining_high_hours"] for r in rows))

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--csv", type=Path, default=ROOT / "docs/RELEASE_PROGRESS.csv")
    p.add_argument("--capacity", type=float, default=8,
                   help="Effective active person-hours per working day across contributors; excludes queue time")
    p.add_argument("--priority", choices=("all","P0","P1"), default="all")
    args = p.parse_args()
    if not math.isfinite(args.capacity) or args.capacity <= 0:
        p.error("capacity must be finite and positive")
    try:
        rows = load(args.csv)
    except (ValueError, KeyError, OSError) as exc:
        p.error(str(exc))
    if args.priority != "all":
        rows = [r for r in rows if r["priority"] == args.priority]
    if not rows:
        p.error("No matching items")
    groups = defaultdict(list)
    for r in rows:
        groups[r["group"]].append(r)
    print("| Nhóm | Nghiệm thu | Chỉ số triển khai | Giờ công còn lại | Ngày công quy đổi |")
    print("|---|---:|---:|---:|---:|")
    for label, subset in list(groups.items()) + [("Tổng", rows)]:
        n, done, pct, low, high = summarize(subset)
        print(f"| {label} | {done}/{n} | {pct:.0f}% | {low}–{high} | {math.ceil(low/args.capacity)}–{math.ceil(high/args.capacity)} |")
    print("\nƯớc lượng kế hoạch, không phải % đã nghiệm thu hoặc ngày cam kết phát hành.")
    print("Chưa tính CI queue, phụ thuộc, máy thật/mẫu thật, và phạm vi tương thích nâng cao chưa định lượng.")

if __name__ == "__main__":
    main()
