"""Fetch a list of approved asset pages in sequence (stdlib only). Each line of the list file: <Folder> <url> [extra urls...]
kenney.nl zip URLs are fetched directly; other hosts go through kg_fetch.py.
  python Tools/Assets/kg_fetch_batch.py <list.txt> <results.jsonl>"""
import json, os, subprocess, sys, time, urllib.request
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FETCH = os.path.join(ROOT, "Tools", "Assets", "kg_fetch.py")
lst, out = sys.argv[1], sys.argv[2]
UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/128.0 Safari/537.36"
with open(out, "a") as log:
    for line in open(lst):
        parts = line.split()
        if not parts or parts[0].startswith("#"):
            continue
        folder, urls = parts[0], parts[1:]
        dest = os.path.join(ROOT, "Art", "Source", folder)
        os.makedirs(dest, exist_ok=True)
        for url in urls:
            t0 = time.time()
            rec = {"folder": folder, "url": url}
            try:
                if "kenney.nl/media" in url:
                    name = url.rsplit("/", 1)[-1]
                    path = os.path.join(dest, name)
                    with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": UA}), timeout=300) as r, open(path, "wb") as f:
                        f.write(r.read())
                    tar = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "tar.exe")
                    subprocess.run([tar, "-xf", path, "-C", dest], capture_output=True)
                    rec["out"] = json.dumps({"file": path, "bytes": os.path.getsize(path)})
                else:
                    r = subprocess.run([sys.executable, FETCH, url, dest], capture_output=True, text=True, timeout=900)
                    rec["out"] = r.stdout[-3000:]
                    rec["err"] = r.stderr[-800:]
                    rec["rc"] = r.returncode
            except Exception as e:
                rec["error"] = repr(e)
            rec["secs"] = round(time.time() - t0, 1)
            if "poly.pizza" in url:
                time.sleep(2.5)   # poly.pizza rate-limits bursts (HTTP 429)
            log.write(json.dumps(rec) + "\n"); log.flush()
            print(json.dumps(rec)[:400], flush=True)
