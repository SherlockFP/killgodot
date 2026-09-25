"""SPRINT-026 evidence: turn the KG_MOVE_CSV lines from a kg_move_smoke.ps1 run into a speed-curve PNG.

    python Tools/Unreal/kg_move_curve.py [log_or_csv] [out_png]

Default input: Saved/Logs/MoveSmokeClient.log (written by kg_move_smoke.ps1). Also writes a plain .csv next to the
PNG (time_s,phase,speed_uu_s) so the raw numbers are inspectable without re-running the engine.

The log lines look like:  LogKillGodot: KG_MOVE_CSV,<t>,<phase>,<speed>
phase is "bad_strafe" (straight line, jump pressed once, never again) or "good_strafe" (alternating strafe + yaw
turn + jump re-pressed every tick, landing inside the ~80ms buffer almost every time - see AKGCharacter::TickMoveSmoke
in Source/KillGodot/Character/KGCharacter.cpp). Two curves on one axis show the contract's acceptance directly: bad
strafe plateaus at/under sprint speed, good strafe climbs measurably above it towards the soft cap.
"""
import csv
import os
import re
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LINE_RE = re.compile(r"KG_MOVE_CSV,([0-9.]+),(\w+),([0-9.]+)")

# Reference lines from Docs/01_GDD_Core.md §4.1 (SprintSpeed=580, BunnyHopSoftCapMultiplier=1.35).
SPRINT_SPEED = 580.0
SOFT_CAP = SPRINT_SPEED * 1.35


def parse(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE_RE.search(line)
            if m:
                rows.append((float(m.group(1)), m.group(2), float(m.group(3))))
    return rows


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Saved", "Logs", "MoveSmokeClient.log")
    out_png = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "Docs", "Level", "SPRINT-026_speed_curve.png")
    out_csv = os.path.splitext(out_png)[0] + ".csv"

    if not os.path.isfile(src):
        print(f"input not found: {src}", file=sys.stderr)
        print("run Tools/Unreal/kg_move_smoke.ps1 first (writes Saved/Logs/MoveSmokeClient.log)", file=sys.stderr)
        sys.exit(1)

    rows = parse(src)
    if not rows:
        print(f"no KG_MOVE_CSV lines found in {src}", file=sys.stderr)
        sys.exit(1)

    os.makedirs(os.path.dirname(out_csv), exist_ok=True)
    with open(out_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["time_s", "phase", "speed_uu_s"])
        w.writerows(rows)

    bad = [(t, s) for t, phase, s in rows if phase == "bad_strafe"]
    good = [(t, s) for t, phase, s in rows if phase == "good_strafe"]

    fig, ax = plt.subplots(figsize=(9, 5), dpi=140)
    if bad:
        ax.plot([t for t, _ in bad], [s for _, s in bad], color="#c0392b", linewidth=2, label="bad strafe (straight, one jump)")
    if good:
        ax.plot([t for t, _ in good], [s for _, s in good], color="#27ae60", linewidth=2, label="good strafe (alternating A/D + yaw + buffered jump)")
    ax.axhline(SPRINT_SPEED, color="#7f8c8d", linestyle="--", linewidth=1, label=f"sprint speed ({SPRINT_SPEED:.0f} uu/s)")
    ax.axhline(SOFT_CAP, color="#2c3e50", linestyle=":", linewidth=1, label=f"soft cap 1.35x ({SOFT_CAP:.0f} uu/s)")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("horizontal speed (uu/s)")
    ax.set_title("SPRINT-026: air-strafe speed curve, good vs bad strafe")
    ax.legend(loc="upper left", fontsize=8)
    ax.grid(alpha=0.25)
    fig.tight_layout()
    fig.savefig(out_png)
    print(f"wrote {out_csv} ({len(rows)} rows) and {out_png}")
    if bad:
        print(f"bad strafe: max {max(s for _, s in bad):.0f} uu/s")
    if good:
        print(f"good strafe: max {max(s for _, s in good):.0f} uu/s")


if __name__ == "__main__":
    main()
