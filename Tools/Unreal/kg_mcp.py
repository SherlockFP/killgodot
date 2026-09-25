"""Minimal Unreal MCP (UE 5.8 ModelContextProtocol plugin) client for scripts and sessions without the MCP connector.

  python Tools/Unreal/kg_mcp.py toolsets
  python Tools/Unreal/kg_mcp.py describe <toolset>
  python Tools/Unreal/kg_mcp.py call <tool> '<json args>' [toolset]
"""
import json
import re
import sys
import urllib.request

URL = "http://localhost:8000/mcp"


def _post(payload, session=None):
    headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    if session:
        headers["Mcp-Session-Id"] = session
    req = urllib.request.Request(URL, data=json.dumps(payload).encode(), headers=headers)
    with urllib.request.urlopen(req, timeout=120) as r:
        body = r.read().decode("utf-8", "replace")
        sid = r.headers.get("Mcp-Session-Id")
    m = re.search(r"\{.*\}", body, re.S)
    return (json.loads(m.group(0)) if m else {}), sid


def session():
    res, sid = _post({"jsonrpc": "2.0", "id": 1, "method": "initialize",
                      "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                                 "clientInfo": {"name": "kg-mcp", "version": "1"}}})
    try:
        _post({"jsonrpc": "2.0", "method": "notifications/initialized"}, sid)
    except Exception:
        pass
    return sid


def call(sid, name, args):
    res, _ = _post({"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": name, "arguments": args}}, sid)
    return res


def main():
    import os
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import kg_lock
    with kg_lock.editor("mcp " + " ".join(sys.argv[1:3])):
        _main()


def _main():
    sid = session()
    cmd = sys.argv[1] if len(sys.argv) > 1 else "toolsets"
    if cmd == "toolsets":
        res = call(sid, "list_toolsets", {})
    elif cmd == "describe":
        res = call(sid, "describe_toolset", {"toolset_name": sys.argv[2]})
    elif cmd == "call":
        args = {"tool_name": sys.argv[2], "arguments": json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}}
        if len(sys.argv) > 4:
            args["toolset_name"] = sys.argv[4]
        res = call(sid, "call_tool", args)
    else:
        raise SystemExit(__doc__)
    content = res.get("result", {}).get("content", [])
    for c in content:
        print(c.get("text", json.dumps(c)))
    if "error" in res:
        print("KG_MCP_ERROR", json.dumps(res["error"]))


if __name__ == "__main__":
    main()
