"""One-shot live iteration against the open editor: StopPIE -> Live Coding -> wait -> StartPIE -> optional probe.

  python Tools/Unreal/kg_live.py                       # recompile .cpp changes and restart PIE
  python Tools/Unreal/kg_live.py --no-compile -p Tools/Unreal/kg_pie_probe.py
  python Tools/Unreal/kg_live.py --shot KG_Look        # restart and take Saved/Screenshots/WindowsEditor/KG_Look.png

Header (.h) / UPROPERTY / new UFUNCTION changes are NOT safe for Live Coding: close the editor, run gates, reopen.
"""
import argparse
import json
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LOG = os.path.join(ROOT, "Saved", "Logs", "KillGodot.log")
PY = sys.executable
HERE = os.path.dirname(os.path.abspath(__file__))
PIE_OPTS = {"options": {"bSimulate": False, "playMode": "PlayMode_InViewPort", "warmupSeconds": 2}}


def sh(*args):
    r = subprocess.run([PY, *args], capture_output=True, text=True, cwd=ROOT)
    return (r.stdout + r.stderr).strip()


def mcp(tool, args):
    return sh(os.path.join(HERE, "kg_mcp.py"), "call", tool, json.dumps(args), "EditorToolset.EditorAppToolset")


def log_lines():
    with open(LOG, encoding="utf-8", errors="replace") as f:
        return f.readlines()


def live_coding(timeout=180):
    start = len(log_lines())
    print(sh(os.path.join(HERE, "kg_remote.py"), "--livecoding"))
    deadline = time.time() + timeout
    while time.time() < deadline:
        time.sleep(2)
        new = [l for l in log_lines()[start:] if "LogLiveCoding" in l or ": error" in l]
        for l in new:
            if "Live coding succeeded" in l or "No changes" in l:
                print("KG_LIVE: compile ok")
                return True
            if "failed" in l.lower() or ": error" in l:
                print("KG_LIVE: compile FAILED\n" + "".join(new[-20:]))
                return False
    print("KG_LIVE: compile timed out")
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-compile", action="store_true")
    ap.add_argument("-p", "--probe", help="python file to run inside the editor after PIE starts")
    ap.add_argument("--shot", help="HighResShot filename (1280x720) after PIE settles")
    ap.add_argument("--settle", type=float, default=1.5)
    a = ap.parse_args()
    mcp("StopPIE", {})
    if not a.no_compile and not live_coding():
        return 1
    print(mcp("StartPIE", PIE_OPTS))
    time.sleep(a.settle)
    if a.probe:
        print(sh(os.path.join(HERE, "kg_remote.py"), "-f", a.probe))
    if a.shot:
        code = ("import unreal\nw = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]\n"
                f"unreal.SystemLibrary.execute_console_command(w, 'HighResShot 1280x720 filename={a.shot}')")
        print(sh(os.path.join(HERE, "kg_remote.py"), "-c", code))
        time.sleep(2.5)
        print("KG_LIVE: shot", os.path.join(ROOT, "Saved", "Screenshots", "WindowsEditor", a.shot + ".png"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
