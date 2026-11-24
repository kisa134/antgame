#!/usr/bin/env python3
"""Tiny stand-in for the Supabase `ingest` function, to test the uploader without a backend.
   python mock_ingest.py <out_folder> [port=8787] [key=testkey] [fail_first=0]
Writes every accepted batch to <out>/batches/*.json and screenshots to <out>/files/<session>/<name>.
Idempotent on (session_id, seq) like the real function; wrong x-ptk-key -> 401; the first `fail_first` requests -> 503."""
import gzip, json, os, sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs

out = sys.argv[1]; port = int(sys.argv[2]) if len(sys.argv) > 2 else 8787
key = sys.argv[3] if len(sys.argv) > 3 else "testkey"; fail_first = int(sys.argv[4]) if len(sys.argv) > 4 else 0
os.makedirs(os.path.join(out, "batches"), exist_ok=True); os.makedirs(os.path.join(out, "files"), exist_ok=True)
seen, nreq = set(), 0


class H(BaseHTTPRequestHandler):
    def _j(self, code, obj):
        b = json.dumps(obj).encode(); self.send_response(code); self.send_header("content-type", "application/json"); self.send_header("content-length", str(len(b))); self.end_headers(); self.wfile.write(b)

    def do_POST(self):
        global nreq
        nreq += 1
        n = int(self.headers.get("content-length", 0)); body = self.rfile.read(n)
        if key != "none" and self.headers.get("x-ptk-key") != key: return self._j(401, {"ok": False, "error": "bad key"})
        if nreq <= fail_first: return self._j(503, {"ok": False, "error": "simulated outage"})
        u = urlparse(self.path)
        if u.path.endswith("/file"):
            q = parse_qs(u.query); d = os.path.join(out, "files", q["session"][0]); os.makedirs(d, exist_ok=True)
            open(os.path.join(d, os.path.basename(q["name"][0])), "wb").write(body)
            print("file", q["session"][0], q["name"][0], len(body)); return self._j(200, {"ok": True})
        if self.headers.get("content-encoding", "") == "gzip": body = gzip.decompress(body)
        b = json.loads(body.decode("utf-8")); sid = b.get("session_id"); new = 0
        for e in b.get("events", []):
            k = (sid, e["seq"])
            if k not in seen: seen.add(k); new += 1
        json.dump(b, open(os.path.join(out, "batches", f"{nreq:05d}_{sid}.json"), "w", encoding="utf-8"), ensure_ascii=False)
        print("batch", sid, "events", len(b.get("events", [])), "new", new, "gz" if self.headers.get("content-encoding") else "plain")
        self._j(200, {"ok": True, "accepted": new})

    def log_message(self, *a): pass


print("mock ingest on", port, "key", key, "fail_first", fail_first, flush=True)
HTTPServer(("127.0.0.1", port), H).serve_forever()
