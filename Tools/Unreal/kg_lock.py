"""Cross-process lock around the live editor, so parallel agents never talk to it at the same time.

Every tool that drives the running editor (kg_remote.py, kg_capture.py, kg_mcp.py) holds this lock for the
duration of its command. Re-entrant inside one process. The OS drops the lock if the holder dies.

  with kg_lock.editor("dress harbour"):
      ...
"""
import contextlib
import msvcrt
import os
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LOCK = os.path.join(ROOT, "Saved", "kg_editor.lock")
INFO = LOCK + ".info"
_depth = 0
_fd = None


@contextlib.contextmanager
def editor(what="", timeout=3600.0):
    global _depth, _fd
    if _depth:
        _depth += 1
        try:
            yield
        finally:
            _depth -= 1
        return
    os.makedirs(os.path.dirname(LOCK), exist_ok=True)
    fd = os.open(LOCK, os.O_RDWR | os.O_CREAT)
    start, told = time.time(), False
    while True:
        try:
            os.lseek(fd, 0, 0)
            msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
            break
        except OSError:
            if time.time() - start > timeout:
                os.close(fd)
                raise TimeoutError(f"KG_LOCK: editor busy for {timeout:.0f}s")
            if not told:
                try:
                    holder = open(INFO, encoding="utf-8").read().strip()
                except OSError:
                    holder = "?"
                print(f"KG_LOCK waiting for the editor (held by: {holder})", flush=True)
                told = True
            time.sleep(0.5)
    try:
        with open(INFO, "w", encoding="utf-8") as f:
            f.write(f"pid {os.getpid()} since {time.strftime('%H:%M:%S')} {what}")
    except OSError:
        pass
    _depth, _fd = 1, fd
    try:
        yield
    finally:
        _depth = 0
        try:
            os.lseek(fd, 0, 0)
            msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
        finally:
            os.close(fd)
            _fd = None
