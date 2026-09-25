"""Kill Godot asset fetcher (stdlib only). Downloads the *free* files of an approved asset page.

Supported sources:
  - itch.io game pages (free / name-your-own-price uploads)
  - opengameart.org content pages (attached files)
  - poly.pizza model pages (.glb)

Safety: only model/archive/texture/audio/font/document extensions are saved; executables and scripts are
refused. Nothing downloaded is ever executed. Archives are extracted with Windows' bsdtar (tar.exe).

Usage:
  python kg_fetch.py <page_url> <dest_dir> [--no-extract]
Prints one JSON line per saved file:  {"file": ..., "bytes": ..., "source": ...}
"""
import html
import http.cookiejar
import json
import os
import re
import subprocess
import sys
import urllib.parse
import urllib.request

UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/128.0 Safari/537.36")
ALLOWED = {".zip", ".7z", ".rar", ".tar", ".gz", ".fbx", ".glb", ".gltf", ".bin", ".obj", ".mtl", ".blend",
           ".dae", ".png", ".jpg", ".jpeg", ".tga", ".txt", ".md", ".pdf", ".wav", ".ogg", ".mp3", ".ttf",
           ".otf", ".unitypackage"}
REFUSED = {".exe", ".msi", ".bat", ".cmd", ".ps1", ".vbs", ".js", ".jar", ".dll", ".scr", ".com", ".sh", ".apk"}
MAX_BYTES = 2 * 1024 ** 3  # 2 GB per file hard cap

_jar = http.cookiejar.CookieJar()
_opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(_jar))


def _req(url, data=None, headers=None):
    h = {"User-Agent": UA}
    h.update(headers or {})
    body = urllib.parse.urlencode(data).encode() if data is not None else None
    return _opener.open(urllib.request.Request(url, data=body, headers=h), timeout=120)


def _get_text(url, **kw):
    with _req(url, **kw) as r:
        return r.read().decode("utf-8", errors="replace")


def _ext(name):
    name = name.lower()
    for double in (".tar.gz",):
        if name.endswith(double):
            return ".gz"
    return os.path.splitext(name)[1]


def _safe_name(name):
    name = os.path.basename(urllib.parse.unquote(name))
    return re.sub(r'[<>:"/\\|?*]', "_", name).strip() or "download.bin"


def _download(url, dest_dir, name, source, headers=None):
    name = _safe_name(name)
    ext = _ext(name)
    if ext in REFUSED or ext not in ALLOWED:
        print(json.dumps({"skipped": name, "reason": f"extension {ext} not allowed"}))
        return None
    path = os.path.join(dest_dir, name)
    total = 0
    with _req(url, headers=headers) as r, open(path, "wb") as f:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            total += len(chunk)
            if total > MAX_BYTES:
                raise RuntimeError(f"{name} exceeds the 2 GB cap")
            f.write(chunk)
    print(json.dumps({"file": path, "bytes": total, "source": source}))
    return path


def fetch_itch(page, dest):
    page = page.rstrip("/")
    text = _get_text(page)
    csrf = re.search(r'name="csrf_token" value="([^"]+)"', text).group(1)
    uploads = re.findall(r'data-upload_id="(\d+)"', text)
    listing = text
    if not uploads:
        resp = json.loads(_get_text(page + "/download_url", data={"csrf_token": csrf},
                                    headers={"X-Requested-With": "XMLHttpRequest", "Referer": page}))
        if "url" not in resp:
            raise RuntimeError(f"itch refused download_url: {resp}")
        listing = _get_text(resp["url"])
        csrf = re.search(r'name="csrf_token" value="([^"]+)"', listing).group(1)
        uploads = re.findall(r'data-upload_id="(\d+)"', listing)
    names = {}
    for block in re.split(r'<div class="upload"', listing)[1:]:
        uid = re.search(r'data-upload_id="(\d+)"', block)
        name = (re.search(r'class="name"[^>]*title="([^"]+)"', block)
                or re.search(r'<strong[^>]*class="name"[^>]*>([^<]+)<', block))
        if uid and name:
            names.setdefault(uid.group(1), html.unescape(name.group(1)).strip())
    saved = []
    for uid in dict.fromkeys(uploads):
        resp = json.loads(_get_text(f"{page}/file/{uid}?source=view_game&as_props=1&after_download_lightbox=true",
                                    data={"csrf_token": csrf},
                                    headers={"X-Requested-With": "XMLHttpRequest", "Referer": page}))
        if "url" not in resp:
            print(json.dumps({"skipped": uid, "reason": resp.get("errors", resp)}))
            continue
        url = resp["url"]
        name = names.get(uid) or urllib.parse.urlparse(url).path.rsplit("/", 1)[-1]
        p = _download(url, dest, name, page)
        if p:
            saved.append(p)
    return saved


def fetch_opengameart(page, dest):
    text = _get_text(page)
    links = re.findall(r'href="(https://opengameart\.org/sites/default/files/[^"]+)"', text)
    saved = []
    for url in dict.fromkeys(links):
        p = _download(url, dest, url.rsplit("/", 1)[-1], page)
        if p:
            saved.append(p)
    return saved


def fetch_polypizza(page, dest):
    text = _get_text(page)
    links = re.findall(r'(https://static\.poly\.pizza/[0-9a-fA-F-]+\.glb)', text)
    if not links:
        raise RuntimeError("no .glb link found on the poly.pizza page")
    slug = page.rstrip("/").rsplit("/", 1)[-1]
    title = re.search(r'property="og:title" content="([^"]+)"', text) or re.search(r"<title>([^<]+)", text)
    raw = html.unescape(title.group(1)).split(" - ")[0].split("|")[0].strip() if title else ""
    base = re.sub(r"[^A-Za-z0-9]+", "_", raw).strip("_") or slug
    return [p for p in [_download(links[0], dest, f"{base}_{slug}.glb", page)] if p]


def extract(archive, dest):
    out = os.path.join(dest, os.path.splitext(os.path.basename(archive))[0])
    os.makedirs(out, exist_ok=True)
    tar = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "tar.exe")
    r = subprocess.run([tar, "-xf", archive, "-C", out], capture_output=True, text=True)
    print(json.dumps({"extracted": archive, "to": out, "ok": r.returncode == 0, "stderr": r.stderr[-300:]}))
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    page, dest = sys.argv[1], sys.argv[2]
    os.makedirs(dest, exist_ok=True)
    host = urllib.parse.urlparse(page).netloc
    if host.endswith("itch.io"):
        saved = fetch_itch(page, dest)
    elif host.endswith("opengameart.org"):
        saved = fetch_opengameart(page, dest)
    elif host.endswith("poly.pizza"):
        saved = fetch_polypizza(page, dest)
    else:
        raise SystemExit(f"unsupported host: {host}")
    if "--no-extract" not in sys.argv:
        for p in saved:
            if _ext(p) in {".zip", ".7z", ".rar", ".tar", ".gz"}:
                extract(p, dest)
    print(json.dumps({"done": page, "files": len(saved)}))


if __name__ == "__main__":
    main()
