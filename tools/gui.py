#!/usr/bin/env python3
# forge gui - a tiny local web dashboard to browse + preview ps2-forge games.
# Lists the examples, and on click it builds one, boots it headless, and shows
# the actual rendered frame + a PASS/FAIL verdict. Serves on :8090 (localhost).
#
#   python3 tools/gui.py            # then open http://localhost:8090
# (needs: ps2dev env on PATH, Play!/AppRun, Xvfb, python with mss + Pillow)
import os, sys, json, glob, time, subprocess, threading, shutil
from urllib.parse import urlparse, parse_qs
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import mss
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = int(os.environ.get("FORGE_GUI_PORT", "8090"))
BOX  = {"left": 64, "top": 19, "width": 842, "height": 560}   # Play! canvas under Xvfb
PLAY = os.environ.get("PLAY") or shutil.which("Play") or os.path.expanduser("~/toolchains/squashfs-root/AppRun")
PREVIEW_DIR = "/tmp/forge_gui"; os.makedirs(PREVIEW_DIR, exist_ok=True)
LOCK = threading.Lock()

def examples():
    d = os.path.join(ROOT, "examples")
    return sorted(n for n in os.listdir(d) if os.path.isdir(os.path.join(d, n)))

def build_and_capture(ex):
    exdir = os.path.join(ROOT, "examples", ex)
    out = os.path.join(PREVIEW_DIR, ex + ".png")
    b = subprocess.run(["make"], cwd=exdir, capture_output=True, text=True)
    elfs = glob.glob(os.path.join(exdir, "*.elf"))
    if b.returncode != 0 or not elfs:
        return {"ok": False, "stage": "build", "log": (b.stderr or b.stdout)[-1500:]}
    elf = elfs[0]
    env = dict(os.environ, DISPLAY=":99")
    # display-scoped only; anything on :99 dies with its Xvfb (don't pkill broad AppRun)
    subprocess.run("pkill -f 'Xvfb :99' 2>/dev/null; sleep 1", shell=True, env=env)
    xv = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1280x720x24", "-nolisten", "tcp"],
                          env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2)
    pl = subprocess.Popen([PLAY, "--elf", elf], env=dict(env, LIBGL_ALWAYS_SOFTWARE="1"),
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(5)
    try:
        with mss.mss(display=":99") as s:
            im = s.grab(BOX)
            img = Image.frombytes("RGB", im.size, im.bgra, "raw", "BGRX")
        img.save(out)
        px = list(img.getdata())
        nb = sum(1 for r, g, b in px if r + g + b > 36)
        cols = len(set((r >> 4, g >> 4, b >> 4) for r, g, b in px))
        frac = nb / len(px)
        verdict = "PASS" if (cols >= 5 and frac > 0.005) else "FAIL"
        return {"ok": True, "verdict": verdict, "distinct": cols,
                "nonblack": round(frac * 100, 1), "img": "/img/%s.png?%d" % (ex, int(time.time()))}
    finally:
        for proc in (pl, xv):
            proc.terminate()
            try: proc.wait(timeout=3)
            except Exception: proc.kill()

PAGE = """<!doctype html><html><head><meta charset=utf-8><title>ps2-forge</title>
<style>
 body{background:#0d0d18;color:#ebeef8;font:15px/1.5 system-ui,sans-serif;margin:0;padding:32px}
 h1{font-weight:800;letter-spacing:.5px} h1 span{color:#cc785c}
 .sub{color:#888fa6;margin-top:-8px}
 .row{display:flex;flex-wrap:wrap;gap:14px;margin:18px 0}
 .gif{width:200px;border:2px solid #2a2c40;border-radius:8px}
 .card{background:#15172a;border:1px solid #262a44;border-radius:12px;padding:16px;width:300px}
 .card h3{margin:0 0 10px;text-transform:uppercase;letter-spacing:1px;font-size:14px;color:#5ac8ff}
 .shot{width:100%;aspect-ratio:842/560;background:#06060e;border-radius:6px;object-fit:contain}
 button{background:#cc785c;color:#120f0c;border:0;border-radius:8px;padding:9px 14px;font-weight:700;cursor:pointer;margin-top:10px}
 button:disabled{opacity:.5} .v{font-weight:800;margin-left:8px}
 .PASS{color:#7fe6a0}.FAIL{color:#ff7a7a} pre{white-space:pre-wrap;color:#ff9a9a;font-size:12px}
</style></head><body>
<h1>ps2&#8209;<span>forge</span></h1>
<div class=sub>build a PlayStation 2 game, see it render. click an example to build + preview it live.</div>
<h3 style="color:#888fa6;margin-top:26px">Showcase (real PS2 output)</h3>
<div class=row id=gifs></div>
<h3 style="color:#888fa6">Examples (build &amp; preview)</h3>
<div class=row id=cards></div>
<script>
const GIFS=%GIFS%, EX=%EX%;
gifs.innerHTML = GIFS.map(g=>`<img class=gif src="/screenshots/${g}">`).join('');
cards.innerHTML = EX.map(e=>`<div class=card><h3>${e}</h3>
  <img class=shot id="s_${e}" src="data:," alt="">
  <div><button id="b_${e}" onclick="prev('${e}')">Build &amp; preview</button>
  <span class=v id="v_${e}"></span></div><pre id="l_${e}"></pre></div>`).join('');
async function prev(e){
  const b=document.getElementById('b_'+e), v=document.getElementById('v_'+e), l=document.getElementById('l_'+e);
  b.disabled=true; v.textContent='building...'; v.className='v'; l.textContent='';
  try{ const r=await (await fetch('/preview?ex='+e,{method:'POST'})).json();
    if(!r.ok){ v.textContent=r.stage+' failed'; v.className='v FAIL'; l.textContent=r.log||''; }
    else{ v.textContent=r.verdict+'  ('+r.distinct+' colors, '+r.nonblack+'% drawn)'; v.className='v '+r.verdict;
      document.getElementById('s_'+e).src=r.img; }
  }catch(err){ v.textContent='error'; v.className='v FAIL'; l.textContent=err; }
  b.disabled=false;
}
</script></body></html>"""

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _send(self, code, ctype, body):
        self.send_response(code); self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
    def do_GET(self):
        p = self.path.split("?")[0]
        if p == "/" or p == "/index.html":
            gifs = [os.path.basename(g) for g in sorted(glob.glob(os.path.join(ROOT, "screenshots", "*.gif")))]
            html = PAGE.replace("%GIFS%", json.dumps(gifs)).replace("%EX%", json.dumps(examples()))
            return self._send(200, "text/html; charset=utf-8", html.encode())
        if p.startswith("/screenshots/"):
            f = os.path.join(ROOT, "screenshots", os.path.basename(p))
            if os.path.exists(f):
                with open(f, "rb") as fh: return self._send(200, "image/gif", fh.read())
        if p.startswith("/img/"):
            f = os.path.join(PREVIEW_DIR, os.path.basename(p))
            if os.path.exists(f):
                with open(f, "rb") as fh: return self._send(200, "image/png", fh.read())
        self._send(404, "text/plain", b"not found")
    def do_POST(self):
        u = urlparse(self.path)
        if u.path == "/preview":
            ex = (parse_qs(u.query).get("ex") or [""])[0]
            if ex not in examples(): return self._send(400, "application/json", b'{"ok":false}')
            with LOCK:
                res = build_and_capture(ex)
            return self._send(200, "application/json", json.dumps(res).encode())
        self._send(404, "text/plain", b"not found")

if __name__ == "__main__":
    print("ps2-forge gui  ->  http://localhost:%d   (emulator: %s)" % (PORT, PLAY))
    # bind localhost only; reach a remote box via: ssh -L PORT:localhost:PORT <host>
    ThreadingHTTPServer(("127.0.0.1", PORT), H).serve_forever()
