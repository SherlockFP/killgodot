"""Download the files of a PUBLIC Google Drive folder (no account, stdlib only; Quaternius' older packs live there).
  python Tools/Assets/kg_fetch_gdrive.py list <folder_id>                      -> prints the recursive tree (id, path, mime)
  python Tools/Assets/kg_fetch_gdrive.py get  <folder_id> <dest_dir> [include_regex]   -> downloads matching files (path match)
Only model/archive/texture/text extensions are saved (same rules as kg_fetch.py); nothing is executed."""
import json, os, re, sys, urllib.request, http.cookiejar
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kg_fetch import ALLOWED, REFUSED, _ext, _safe_name
UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/128.0 Safari/537.36"
jar = http.cookiejar.CookieJar()
opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

def _get(url):
    return opener.open(urllib.request.Request(url, headers={"User-Agent": UA}), timeout=300)

def walk(fid, path=""):
    t = _get(f"https://drive.google.com/drive/folders/{fid}").read().decode("utf-8", "replace").replace("\\", "")
    for i, name, mime in re.findall(r'x5bx22([\w-]{20,})x22,x5bx22[\w-]+x22x5d,x22(.+?)x22,x22(.+?)x22', t):
        name = name.encode().decode("unicode_escape", "ignore") if "u00" in name else name
        p = f"{path}/{name}" if path else name
        if mime.endswith("folder"):
            yield from walk(i, p)
        else:
            yield i, p, mime

def download(i, dest):
    url = f"https://drive.google.com/uc?export=download&id={i}&confirm=t"
    r = _get(url)
    ct = r.headers.get("Content-Type", "")
    if "text/html" in ct:   # virus-scan interstitial for big files: submit its form
        h = r.read().decode("utf-8", "replace")
        act = re.search(r'action="([^"]+)"', h)
        params = dict(re.findall(r'name="(\w+)" value="([^"]*)"', h))
        r = _get(act.group(1) + "?" + urllib.parse.urlencode(params))
    total = 0
    with r, open(dest, "wb") as f:
        while True:
            c = r.read(1 << 20)
            if not c: break
            f.write(c); total += len(c)
    return total

if __name__ == "__main__":
    import urllib.parse
    cmd, fid = sys.argv[1], sys.argv[2]
    if cmd == "list":
        for i, p, m in walk(fid):
            print(i, "|", p, "|", m)
    else:
        dest, inc = sys.argv[3], (sys.argv[4] if len(sys.argv) > 4 else ".")
        for i, p, m in walk(fid):
            if not re.search(inc, p, re.I): continue
            ext = _ext(p)
            if ext in REFUSED or ext not in ALLOWED:
                print(json.dumps({"skipped": p, "reason": f"extension {ext}"})); continue
            out = os.path.join(dest, *[_safe_name(x) for x in p.split("/")])
            os.makedirs(os.path.dirname(out), exist_ok=True)
            n = download(i, out)
            print(json.dumps({"file": out, "bytes": n, "source": f"https://drive.google.com/drive/folders/{fid}"}), flush=True)
