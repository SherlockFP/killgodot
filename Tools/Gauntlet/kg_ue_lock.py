"""Serialises everything that uses the UE binaries (C++ builds, run_gates/run_invariants, -game smokes and match batches,
UnrealEditor-Cmd commandlets) across parallel agents. A running -game process holds the module DLLs, so a build started
next to it fails with LNK1104; two commandlets saving the same .umap fail silently. One holder at a time.

  python Tools/Gauntlet/kg_ue_lock.py "<who: what>" -- <command> [args...]

Runs the command while holding Saved/kg_ue.lock and returns its exit code. The OS drops the lock if the holder dies.
Keep each hold short (a build, one smoke, a chunk of <= 4 matches) so the others get a turn.
"""
import msvcrt
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LOCK = os.path.join(ROOT, "Saved", "kg_ue.lock")
INFO = LOCK + ".info"


def main():
    if "--" not in sys.argv:
        print(__doc__)
        return 2
    cut = sys.argv.index("--")
    what = " ".join(sys.argv[1:cut]) or "?"
    cmd = sys.argv[cut + 1:]
    os.makedirs(os.path.dirname(LOCK), exist_ok=True)
    fd = os.open(LOCK, os.O_RDWR | os.O_CREAT)
    start, told = time.time(), False
    while True:
        try:
            os.lseek(fd, 0, 0)
            msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
            break
        except OSError:
            if not told:
                try:
                    holder = open(INFO, encoding="utf-8").read().strip()
                except OSError:
                    holder = "?"
                print(f"KG_UE_LOCK waiting (held by: {holder})", flush=True)
                told = True
            time.sleep(1.0)
    waited = time.time() - start
    try:
        with open(INFO, "w", encoding="utf-8") as f:
            f.write(f"pid {os.getpid()} since {time.strftime('%H:%M:%S')} {what}")
    except OSError:
        pass
    print(f"KG_UE_LOCK acquired after {waited:.0f}s: {what}", flush=True)
    try:
        return subprocess.call(cmd, cwd=ROOT)
    finally:
        os.lseek(fd, 0, 0)
        msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
        os.close(fd)


if __name__ == "__main__":
    sys.exit(main())
