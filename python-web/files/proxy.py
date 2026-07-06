"""
Air Station Proxy Server
- Serve file HTML tai http://localhost:8080
- Chuyen tiep request AI qua /ai -> Gemini API (khong bi CORS)
- Rate-limit phia server: 1 request / 60 giay

Cach chay: python proxy.py
"""
from http.server import HTTPServer, SimpleHTTPRequestHandler
import json, urllib.request, urllib.error, time, os

# Thu muc chua proxy.py (va AIRSTATION.html) — dam bao serve dung cho
SERVE_DIR = os.path.dirname(os.path.abspath(__file__))

# ---- DAN API KEY GEMINI CUA BAN VAO DAY (key AIza...) ----
GEMINI_KEY = "AIzaSyDFuSCY2mGV4gW_iz9ECEzNRGTd1O3w7G4"
GEMINI_MODEL = "gemini-2.5-flash-lite"
# -----------------------------------------------------------

# Rate-limit phia server (khong phu thuoc reload trang)
COOLDOWN = 60          # giay
_last_call = 0

class Handler(SimpleHTTPRequestHandler):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=SERVE_DIR, **kwargs)

    def do_OPTIONS(self):
        self.send_response(200)
        self._cors()
        self.end_headers()

    def do_POST(self):
        global _last_call

        if self.path != '/ai':
            self.send_error(404)
            return

        # --- Rate-limit ---
        now = time.time()
        if now - _last_call < COOLDOWN:
            wait = int(COOLDOWN - (now - _last_call))
            out = json.dumps({"ok": False, "error": f"Rate limit: thu lai sau {wait}s"}).encode()
            self.send_response(429)
            self.send_header("Content-Type", "application/json")
            self._cors()
            self.end_headers()
            self.wfile.write(out)
            return
        _last_call = now

        # Doc body tu browser
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length)
        prompt = json.loads(body).get('prompt', '')

        # Tao request gui Gemini
        url = f"https://generativelanguage.googleapis.com/v1beta/models/{GEMINI_MODEL}:generateContent?key={GEMINI_KEY}"
        payload = json.dumps({
            "contents": [{"parts": [{"text": prompt}]}]
        }).encode()

        req = urllib.request.Request(url, data=payload, headers={
            "Content-Type": "application/json"
        }, method="POST")

        try:
            with urllib.request.urlopen(req) as resp:
                result = json.loads(resp.read())
            text = result["candidates"][0]["content"]["parts"][0]["text"]
            reply = {"ok": True, "text": text}
        except urllib.error.HTTPError as e:
            err = e.read().decode()
            reply = {"ok": False, "error": err}
        except Exception as e:
            reply = {"ok": False, "error": str(e)}

        out = json.dumps(reply).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self._cors()
        self.end_headers()
        self.wfile.write(out)

    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

print(f"=== Air Station Server ===")
print(f"Mo trinh duyet: http://localhost:8080/AIRSTATION.html")
print(f"Gemini model:   {GEMINI_MODEL}")
print(f"Nhan Ctrl+C de dung.\n")
HTTPServer(("", 8080), Handler).serve_forever()