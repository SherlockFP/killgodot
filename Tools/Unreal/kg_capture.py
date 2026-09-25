"""Capture the editor viewport from named camera views through the UE MCP server (works without PIE).

  python Tools/Unreal/kg_capture.py aerial plaza street     -> Saved/Screenshots/KG_Cap_<view>.png
  python Tools/Unreal/kg_capture.py custom:X,Y,Z,PITCH,YAW  -> Saved/Screenshots/KG_Cap_custom.png
  python Tools/Unreal/kg_capture.py shot:harbour_1:X,Y,Z,PITCH,YAW  -> Saved/Screenshots/KG_Cap_harbour_1.png
"""
import base64
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kg_lock  # noqa: E402
import kg_mcp  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "Saved", "Screenshots")
VIEWS = {
    "aerial": ((-9000, 9000, 6500), (-28, -45)),
    "harbour": ((1500, 11000, 1200), (-10, -105)),
    "plaza": ((0, 1600, 620), (-6, -90)),
    "street": ((250, 4800, 560), (-3, -92)),
    "house": ((-2300, 1300, 560), (-4, -60)),
    "hills": ((0, -2500, 900), (2, -90)),
    "top": ((0, 1500, 16000), (-89, -90)),
}


def capture(sid, name, loc, rot):
    args = {"tool_name": "CaptureViewport", "toolset_name": "EditorToolset.EditorAppToolset", "arguments": {
        "captureTransform": {"location": dict(zip("xyz", loc)), "rotation": {"pitch": rot[0], "yaw": rot[1], "roll": 0},
                             "scale": {"x": 1, "y": 1, "z": 1}},
        "annotations": None, "bShowUI": False}}
    res = kg_mcp.call(sid, "call_tool", args)
    for c in res.get("result", {}).get("content", []):
        text = c.get("text", "")
        try:
            data = json.loads(text)
        except ValueError:
            continue
        img = data.get("returnValue", {}).get("image", {}).get("data")
        if img:
            os.makedirs(OUT, exist_ok=True)
            path = os.path.join(OUT, f"KG_Cap_{name}.png")
            open(path, "wb").write(base64.b64decode(img))
            return path
        if c.get("type") == "image":
            path = os.path.join(OUT, f"KG_Cap_{name}.png")
            open(path, "wb").write(base64.b64decode(c["data"]))
            return path
    print("KG_CAPTURE failed", name, json.dumps(res)[:400])
    return None


def main():
    with kg_lock.editor("capture " + " ".join(sys.argv[1:])):
        _main()


def _main():
    sid = kg_mcp.session()
    for arg in sys.argv[1:] or ["aerial"]:
        if arg.startswith("custom:"):
            x, y, z, p, yw = (float(v) for v in arg[7:].split(","))
            name, loc, rot = "custom", (x, y, z), (p, yw)
        elif arg.startswith("shot:"):
            # shot:<name>:X,Y,Z,PITCH,YAW -> KG_Cap_<name>.png (unique names: parallel agents share the folder)
            _, name, nums = arg.split(":", 2)
            x, y, z, p, yw = (float(v) for v in nums.split(","))
            loc, rot = (x, y, z), (p, yw)
        else:
            name, (loc, rot) = arg, VIEWS[arg]
        print("KG_CAPTURE", name, capture(sid, name, loc, rot))


if __name__ == "__main__":
    main()
