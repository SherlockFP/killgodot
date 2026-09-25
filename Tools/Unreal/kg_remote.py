"""Run Python inside the *running* Unreal Editor (Python Remote Execution, localhost).

  python Tools/Unreal/kg_remote.py -c "import unreal; print(unreal.SystemLibrary.get_engine_version())"
  python Tools/Unreal/kg_remote.py -f Tools/Unreal/some_script.py
  python Tools/Unreal/kg_remote.py --livecoding        # trigger a Live Coding compile

Requires [/Script/PythonScriptPlugin.PythonScriptPluginSettings] bRemoteExecution=True (Config/DefaultEngine.ini).
"""
import argparse
import sys
import time

ENGINE_PY = r"D:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python"
sys.path.insert(0, ENGINE_PY)
import remote_execution as rx  # noqa: E402

sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
import kg_lock  # noqa: E402


def run(command, exec_mode, timeout=30.0):
    # One editor, many agents: serialize every remote command (kg_lock.py).
    with kg_lock.editor(command.strip().splitlines()[0][:80] if command.strip() else "remote"):
        return _run(command, exec_mode, timeout)


def _run(command, exec_mode, timeout=30.0):
    cfg = rx.RemoteExecutionConfig()
    cfg.multicast_bind_address = "127.0.0.1"
    remote = rx.RemoteExecution(cfg)
    remote.start()
    try:
        deadline = time.time() + timeout
        node = None
        while time.time() < deadline and node is None:
            nodes = [n for n in remote.remote_nodes if "KillGodot" in str(n.get("project_name", ""))] or remote.remote_nodes
            node = nodes[0] if nodes else None
            if node is None:
                time.sleep(0.5)
        if node is None:
            print("KG_REMOTE: no editor found (is it open with bRemoteExecution=True?)")
            return 2
        remote.open_command_connection(node["node_id"])
        result = remote.run_command(command, unattended=True, exec_mode=exec_mode)
        for line in result.get("output", []):
            print(line.get("output", "").rstrip())
        if not result.get("success", False):
            print(f"KG_REMOTE: FAILED {result.get('result')}")
            return 1
        if exec_mode == rx.MODE_EVAL_STATEMENT:
            print(result.get("result"))
        return 0
    finally:
        remote.stop()


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("-c", "--command")
    g.add_argument("-f", "--file")
    g.add_argument("--livecoding", action="store_true")
    ap.add_argument("--timeout", type=float, default=30.0)
    a = ap.parse_args()
    if a.livecoding:
        code = "import unreal\nunreal.SystemLibrary.execute_console_command(None, 'LiveCoding.Compile')\nprint('KG_LIVECODING requested')"
        return run(code, rx.MODE_EXEC_FILE, a.timeout)
    if a.file:
        code = open(a.file, encoding="utf-8").read()
        return run(code, rx.MODE_EXEC_FILE, a.timeout)
    return run(a.command, rx.MODE_EXEC_FILE, a.timeout)


if __name__ == "__main__":
    sys.exit(main())
