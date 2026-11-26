# -*- coding: utf-8 -*-
"""Ant Kitchen Launcher - backend (pywebview + requests). Portable paths, Windows-first."""
import io, struct, zlib, os, sys, json, time, threading, shutil, zipfile, hashlib, subprocess, webbrowser, re, shlex, datetime, tempfile, collections

try:
    import requests
except Exception:  # pragma: no cover
    requests = None

VERSION = "1.0.2"
REPO = "kisa134/antgame"
API_URL = os.environ.get("AK_API") or "https://api.github.com/repos/%s/releases?per_page=30" % REPO
ZIP_RE = re.compile(r"^AntKitchen-playtest-.*\.zip$")
IS_WIN = os.name == "nt"
IS_MAC = sys.platform == "darwin"
UA = {"User-Agent": "AntKitchenLauncher/" + VERSION}
GH_HEAD = dict(UA, Accept="application/vnd.github+json")
CREATE_NO_WINDOW = 0x08000000 if IS_WIN else 0


def resource(rel):
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, rel)


def cfg_dir():
    if os.environ.get("AK_HOME"):
        return os.environ["AK_HOME"]
    if IS_WIN:
        return os.path.join(os.environ.get("APPDATA", os.path.expanduser("~")), "AntKitchenLauncher")
    if IS_MAC:
        return os.path.expanduser("~/Library/Application Support/AntKitchenLauncher")
    return os.path.expanduser("~/.config/AntKitchenLauncher")


def default_root():
    if os.environ.get("AK_ROOT"):
        return os.environ["AK_ROOT"]
    if IS_WIN:
        return os.path.join(os.environ.get("LOCALAPPDATA", os.path.expanduser("~")), "AntKitchen")
    if IS_MAC:
        return os.path.expanduser("~/Library/Application Support/AntKitchen")
    return os.path.expanduser("~/.local/share/AntKitchen")


def fmt_mb(b):
    return "%.0f МБ" % (b / 1e6)


def lp(p):
    """long-path prefix on Windows"""
    p = os.path.abspath(p)
    if IS_WIN and not p.startswith("\\\\?\\"):
        return "\\\\?\\" + p
    return p


def desktop_dir():
    if IS_WIN:
        try:
            import ctypes
            buf = ctypes.create_unicode_buffer(260)
            ctypes.windll.shell32.SHGetFolderPathW(None, 0x10, None, 0, buf)
            if buf.value:
                return buf.value
        except Exception:
            pass
    return os.path.join(os.path.expanduser("~"), "Desktop")


def ver_tuple(s):
    return tuple(int(x) for x in re.findall(r"\d+", s or "0")[:4]) or (0,)


# ---------------------------------------------------------------- processes
def list_procs(prefixes):
    """[(pid, name, path)] for processes whose exe name starts with one of prefixes (lowercase)."""
    out = []
    if IS_WIN:
        import ctypes
        from ctypes import wintypes

        class PE(ctypes.Structure):
            _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD), ("th32ProcessID", wintypes.DWORD),
                        ("th32DefaultHeapID", ctypes.c_void_p), ("th32ModuleID", wintypes.DWORD),
                        ("cntThreads", wintypes.DWORD), ("th32ParentProcessID", wintypes.DWORD),
                        ("pcPriClassBase", ctypes.c_long), ("dwFlags", wintypes.DWORD),
                        ("szExeFile", ctypes.c_wchar * 260)]
        k = ctypes.windll.kernel32
        k.CreateToolhelp32Snapshot.restype = ctypes.c_void_p
        k.OpenProcess.restype = ctypes.c_void_p
        k.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        k.CloseHandle.argtypes = [ctypes.c_void_p]
        k.QueryFullProcessImageNameW.argtypes = [ctypes.c_void_p, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        snap = k.CreateToolhelp32Snapshot(0x2, 0)
        pe = PE()
        pe.dwSize = ctypes.sizeof(PE)
        ok = k.Process32FirstW(ctypes.c_void_p(snap), ctypes.byref(pe))
        while ok:
            name = pe.szExeFile
            if name.lower().startswith(prefixes):
                path = ""
                h = k.OpenProcess(0x1000, False, pe.th32ProcessID)
                if h:
                    buf = ctypes.create_unicode_buffer(1024)
                    n = wintypes.DWORD(1024)
                    if k.QueryFullProcessImageNameW(h, 0, buf, ctypes.byref(n)):
                        path = buf.value
                    k.CloseHandle(h)
                out.append((pe.th32ProcessID, name, path))
            ok = k.Process32NextW(ctypes.c_void_p(snap), ctypes.byref(pe))
        k.CloseHandle(ctypes.c_void_p(snap))
    else:
        try:
            r = subprocess.run(["ps", "-axo", "pid=,args="], capture_output=True, text=True, timeout=5)
            for line in r.stdout.splitlines():
                pid, _, args = line.strip().partition(" ")
                base = os.path.basename(args.split(" ")[0]).lower()
                if base.startswith(prefixes):
                    out.append((int(pid), base, args))
        except Exception:
            pass
    return out


def steam_status():
    """returns one of: ok, not_running, not_installed, not_logged_in"""
    if IS_WIN:
        import winreg
        path = None
        for hive, sub in ((winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam"), (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam")):
            try:
                with winreg.OpenKey(hive, sub) as k:
                    path = winreg.QueryValueEx(k, "SteamPath" if hive == winreg.HKEY_CURRENT_USER else "InstallPath")[0]
                    break
            except OSError:
                continue
        if not path or not os.path.exists(path):
            return "not_installed"
        if not list_procs(("steam.exe",)):
            return "not_running"
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam\ActiveProcess") as k:
                if winreg.QueryValueEx(k, "ActiveUser")[0] == 0:
                    return "not_logged_in"
        except OSError:
            pass
        return "ok"
    if list_procs(("steam",)):
        return "ok"
    return "not_running" if os.path.exists("/Applications/Steam.app") else "not_installed"


def webview2_ok():
    if not IS_WIN:
        return True
    import winreg
    guid = r"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
    for hive, sub in ((winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\\" + guid),
                      (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\EdgeUpdate\Clients\\" + guid),
                      (winreg.HKEY_CURRENT_USER, r"Software\Microsoft\EdgeUpdate\Clients\\" + guid)):
        try:
            with winreg.OpenKey(hive, sub) as k:
                if winreg.QueryValueEx(k, "pv")[0] not in ("", "0.0.0.0"):
                    return True
        except OSError:
            continue
    return False


def vcrt_ok():
    """UE prerequisite = VC++ 2015-2022 x64 runtime"""
    if not IS_WIN:
        return True
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64") as k:
            return winreg.QueryValueEx(k, "Installed")[0] == 1
    except OSError:
        return False


class _Cancel(Exception):
    pass


def _range(url, a, b, tries=4):
    last = None
    for i in range(tries):
        try:
            r = requests.get(url, headers=dict(UA, Range="bytes=%d-%d" % (a, b)), timeout=(15, 40))
            if r.status_code != 206:
                raise IOError("HTTP %d (Range)" % r.status_code)
            return r.content
        except (requests.RequestException, IOError) as e:
            last = e
            time.sleep(1 + i)
    raise last


class RangeFile(io.RawIOBase):
    """Read-only file object over HTTP Range requests: lets zipfile read the central directory of a remote zip."""
    def __init__(self, url, size):
        self.url, self.size, self.pos = url, size, 0

    def seekable(self):
        return True

    def readable(self):
        return True

    def tell(self):
        return self.pos

    def seek(self, off, whence=0):
        self.pos = off if whence == 0 else (self.pos + off if whence == 1 else self.size + off)
        return self.pos

    def read(self, n=-1):
        if n < 0 or self.pos + n > self.size:
            n = self.size - self.pos
        if n <= 0:
            return b""
        d = _range(self.url, self.pos, self.pos + n - 1)
        self.pos += len(d)
        return d

    def readall(self):
        return self.read(-1)


def file_crc(path):
    c = 0
    with open(path, "rb") as f:
        while True:
            b = f.read(8 << 20)
            if not b:
                break
            c = zlib.crc32(b, c)
    return c & 0xFFFFFFFF


# ---------------------------------------------------------------- API
class Core:
    def __init__(self):
        self.window = None
        os.makedirs(cfg_dir(), exist_ok=True)
        self.cfg_path = os.path.join(cfg_dir(), "config.json")
        self.cfg = {"root": default_root(), "channel": "latest", "pinned_tag": "", "installed": "", "previous": "",
                    "prereq_done": False, "platform": "auto", "chat_url": "", "opts": {"tutorial": False, "admin": False, "windowed": False, "args": ""}}
        try:
            with open(self.cfg_path, "r", encoding="utf-8") as f:
                self.cfg.update(json.load(f))
        except Exception:
            pass
        self.lock = threading.RLock()
        self.releases = []
        self.rel_status = "loading"
        self.rel_error = ""
        self.task = None          # dict while a job runs
        self.stop_flag = None     # None / 'pause' / 'cancel'
        self.launching_until = 0
        self.selfupd = {"available": False, "version": "", "url": "", "busy": False}
        self.notice = ""
        self.delta_info = None
        try:
            with open(os.path.join(cfg_dir(), "releases_cache.json"), "r", encoding="utf-8") as f:
                self.releases = json.load(f)
        except Exception:
            pass
        threading.Thread(target=self.refresh, daemon=True).start()

    # ---- config
    def save_cfg(self):
        with self.lock:
            tmp = self.cfg_path + ".tmp"
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(self.cfg, f, ensure_ascii=False, indent=1)
            os.replace(tmp, self.cfg_path)

    def root(self):
        return self.cfg["root"]

    def builds_dir(self):
        return os.path.join(self.root(), "builds")

    def build_dir(self, tag):
        return os.path.join(self.builds_dir(), tag)

    def game_exe(self, tag=None):
        tag = tag or self.cfg.get("installed")
        if not tag:
            return ""
        p = os.path.join(self.build_dir(tag), "Windows", "AntKitchen.exe")
        return p if os.path.exists(p) else ""

    def saved_dir(self, tag=None):
        tag = tag or self.cfg.get("installed")
        return os.path.join(self.build_dir(tag), "Windows", "AntKitchen", "Saved") if tag else ""

    def installed_tags(self):
        out = []
        try:
            for n in os.listdir(self.builds_dir()):
                if os.path.exists(os.path.join(self.builds_dir(), n, ".complete")):
                    out.append(n)
        except OSError:
            pass
        return out

    # ---- releases
    def refresh(self):
        self.rel_status = "loading"
        try:
            r = requests.get(API_URL, headers=GH_HEAD, timeout=15)
            r.raise_for_status()
            rels = []
            for x in r.json():
                if x.get("draft"):
                    continue
                assets = {a["name"]: a for a in x.get("assets", [])}
                zipa = next((a for a in x.get("assets", []) if ZIP_RE.match(a["name"])), None)
                item = {"tag": x["tag_name"], "name": x.get("name") or x["tag_name"], "body": x.get("body") or "",
                        "created": x.get("created_at") or "", "published": x.get("published_at") or x.get("created_at") or "",
                        "prerelease": bool(x.get("prerelease")), "url": x.get("html_url"), "zip": None, "launcher": None, "launcher_ver": None}
                if zipa:
                    sha = assets.get(zipa["name"] + ".sha256")
                    item["zip"] = {"name": zipa["name"], "size": zipa["size"], "url": zipa["browser_download_url"],
                                   "digest": (zipa.get("digest") or "").replace("sha256:", ""), "sha_url": sha["browser_download_url"] if sha else ""}
                if "AntKitchenLauncher.exe" in assets:
                    item["launcher"] = assets["AntKitchenLauncher.exe"]["browser_download_url"]
                if "launcher-version.txt" in assets:
                    item["launcher_ver"] = assets["launcher-version.txt"]["browser_download_url"]
                rels.append(item)
            # newest first by created_at (tags like v14.1 are not parsed)
            rels.sort(key=lambda i: i["created"], reverse=True)
            with self.lock:
                self.releases = rels
            with open(os.path.join(cfg_dir(), "releases_cache.json"), "w", encoding="utf-8") as f:
                json.dump(rels, f, ensure_ascii=False)
            self.rel_status = "ok"
            self.rel_error = ""
            self._check_self_update()
        except Exception as e:
            self.rel_status = "error"
            self.rel_error = str(e)[:200]

    def target_release(self):
        with self.lock:
            rels = [r for r in self.releases if r["zip"]]
        if self.cfg.get("channel") == "pinned" and self.cfg.get("pinned_tag"):
            for r in rels:
                if r["tag"] == self.cfg["pinned_tag"]:
                    return r
        return rels[0] if rels else None

    def _check_self_update(self):
        try:
            for r in self.releases:
                if r.get("launcher") and r.get("launcher_ver"):
                    v = requests.get(r["launcher_ver"], headers=UA, timeout=10).text.strip()
                    self.selfupd.update(version=v, url=r["launcher"], available=ver_tuple(v) > ver_tuple(VERSION))
                    return
        except Exception:
            pass

    # ---- state for UI
    def get_state(self):
        tgt = self.target_release()
        inst = self.cfg.get("installed")
        inst_ok = bool(self.game_exe())
        free = 0
        try:
            p = self.root()
            while p and not os.path.exists(p):
                p = os.path.dirname(p)
            free = shutil.disk_usage(p or ".").free
        except Exception:
            pass
        procs = []
        try:
            procs = [p for p in list_procs(("antkitchen",)) if self.builds_dir().lower() in (p[2] or "").lower()]
        except Exception:
            pass
        running = bool(procs)
        with self.lock:
            task = dict(self.task) if self.task else None
            rels = list(self.releases)
        auto = "windows" if IS_WIN else ("mac" if IS_MAC else "other")
        return {
            "version": VERSION, "auto_platform": auto, "platform": self.cfg.get("platform", "auto"),
            "root": self.root(), "installed": inst if inst_ok else "", "previous": self.cfg.get("previous", ""),
            "installed_tags": self.installed_tags(),
            "target": ({"tag": tgt["tag"], "name": tgt["name"], "size": tgt["zip"]["size"], "published": tgt["published"]} if tgt else None),
            "update_available": bool(tgt and inst_ok and tgt["tag"] != inst),
            "rel_status": self.rel_status, "rel_error": self.rel_error,
            "releases": [{"tag": r["tag"], "name": r["name"], "body": r["body"], "published": r["published"], "url": r["url"],
                          "size": (r["zip"] or {}).get("size", 0), "has_zip": bool(r["zip"])} for r in rels],
            "channel": self.cfg.get("channel"), "pinned_tag": self.cfg.get("pinned_tag"),
            "steam": steam_status() if IS_WIN or IS_MAC else "not_running",
            "prereq": "ok" if (self.cfg.get("prereq_done") or vcrt_ok()) else "missing",
            "running": running, "launching": time.time() < self.launching_until and not running,
            "free": free, "task": task, "opts": self.cfg.get("opts"),
            "selfupdate": dict(self.selfupd), "frozen": bool(getattr(sys, "frozen", False)),
            "notice": self.notice, "chat_url": self.cfg.get("chat_url", ""),
        }

    # ---- settings
    def set_opts(self, opts):
        self.cfg["opts"] = opts
        self.save_cfg()
        return True

    def set_chat_url(self, u):
        self.cfg["chat_url"] = u
        self.save_cfg()
        return True

    def set_platform(self, p):
        self.cfg["platform"] = p
        self.save_cfg()
        return True

    def set_channel(self, channel, tag=""):
        self.cfg["channel"] = channel
        self.cfg["pinned_tag"] = tag
        self.save_cfg()
        return True

    def set_root(self, path):
        if self.task:
            return {"ok": False, "error": "Идёт загрузка"}
        path = os.path.abspath(path)
        try:
            os.makedirs(path, exist_ok=True)
        except OSError as e:
            return {"ok": False, "error": str(e)}
        self.cfg.update(root=path, installed="", previous="")
        # if builds already exist there, adopt newest
        tags = self.installed_tags()
        if tags:
            tags.sort(key=lambda t: os.path.getmtime(os.path.join(self.builds_dir(), t, ".complete")), reverse=True)
            self.cfg["installed"] = tags[0]
            self.cfg["previous"] = tags[1] if len(tags) > 1 else ""
        self.save_cfg()
        return {"ok": True}

    def choose_root(self):
        import webview
        res = self.window.create_file_dialog(webview.FOLDER_DIALOG, directory=self.root())
        if res:
            return self.set_root(os.path.join(res[0], "AntKitchen") if not os.path.basename(res[0]).lower().startswith("antkitchen") else res[0])
        return {"ok": False, "error": ""}

    # ---- misc helpers
    def open_url(self, url):
        if url.startswith(("http://", "https://", "steam://")):
            if url.startswith("steam://") and IS_WIN:
                os.startfile(url)
            else:
                webbrowser.open(url)
        return True

    def open_path(self, which):
        p = {"root": self.root(), "saved": self.saved_dir(), "logs": os.path.join(self.saved_dir() or "", "Logs") if self.saved_dir() else "",
             "launcher": cfg_dir(), "desktop": desktop_dir()}.get(which, "")
        while p and not os.path.exists(p):
            p = os.path.dirname(p)
        if not p:
            return False
        if IS_WIN:
            os.startfile(p)
        elif IS_MAC:
            subprocess.Popen(["open", p])
        else:
            subprocess.Popen(["xdg-open", p])
        return True

    def copy_text(self, text):
        try:
            if IS_WIN:
                subprocess.run(["clip"], input=("﻿" + text).encode("utf-16le"), creationflags=CREATE_NO_WINDOW, timeout=5)
            elif IS_MAC:
                subprocess.run(["pbcopy"], input=text.encode("utf-8"), timeout=5)
            return True
        except Exception:
            return False

    def start_steam(self):
        try:
            if IS_WIN:
                os.startfile("steam://open/main")
            else:
                webbrowser.open("steam://open/main")
            return True
        except Exception:
            return False

    # ---- install / update
    def _set_task(self, **kw):
        with self.lock:
            if self.task is None:
                self.task = {}
            self.task.update(kw)

    def install(self, tag=None):
        with self.lock:
            if self.task and self.task.get("phase") not in ("error", "paused", "done"):
                return {"ok": False, "error": "Уже идёт задача"}
        rel = None
        if tag:
            rel = next((r for r in self.releases if r["tag"] == tag and r["zip"]), None)
        else:
            rel = self.target_release()
        if not rel:
            return {"ok": False, "error": "Нет доступных сборок (проверь интернет)"}
        self.stop_flag = None
        self.task = {"tag": rel["tag"], "phase": "download", "pct": 0, "bytes": 0, "total": rel["zip"]["size"], "speed": 0, "eta": 0, "msg": "Подключаемся…", "error": ""}
        threading.Thread(target=self._install_job, args=(rel,), daemon=True).start()
        return {"ok": True}

    def pause(self):
        self.stop_flag = "pause"
        return True

    def cancel(self):
        self.stop_flag = "cancel"
        if self.task and self.task.get("phase") in ("paused", "error"):
            self._cleanup_part(self.task.get("tag"))
            self.task = None
        return True

    def dismiss(self):
        with self.lock:
            if self.task and self.task.get("phase") in ("error", "done"):
                self.task = None
        return True

    def _part_paths(self, rel):
        d = os.path.join(self.root(), "downloads")
        os.makedirs(d, exist_ok=True)
        return os.path.join(d, rel["zip"]["name"] + ".part"), os.path.join(d, rel["zip"]["name"])

    def _cleanup_part(self, tag):
        try:
            rel = next((r for r in self.releases if r["tag"] == tag), None)
            if rel and rel["zip"]:
                for p in self._part_paths(rel):
                    if os.path.exists(p):
                        os.remove(p)
        except Exception:
            pass

    def _install_job(self, rel):
        tag = rel["tag"]
        z = rel["zip"]
        try:
            part, final = self._part_paths(rel)
            have = os.path.getsize(part) if os.path.exists(part) else 0
            old = self.cfg.get("installed")
            if old and old != tag and not have and not os.path.exists(final) and os.path.exists(os.path.join(self.build_dir(old), ".complete")) \
                    and not os.environ.get("AK_NODELTA"):
                try:
                    if self._delta(rel, old):
                        self._activate(tag)
                        self._set_task(phase="done", pct=100, msg="Готово (дельта-обновление)")
                        return
                except _Cancel:
                    shutil.rmtree(lp(self.build_dir(tag) + ".partial"), ignore_errors=True)
                    self.task = None
                    return
                except Exception:
                    shutil.rmtree(lp(self.build_dir(tag) + ".partial"), ignore_errors=True)
                self.stop_flag = None
                self._set_task(phase="download", pct=0, msg="Скачиваем полную сборку…")
            if not os.path.exists(final):
                need = int(z["size"] * 3.2) - have
                free = shutil.disk_usage(self.root()).free
                if free < need:
                    raise RuntimeError("Мало места на диске: нужно ~%.1f ГБ, свободно %.1f ГБ. Освободи место или выбери другую папку в Настройках." % (need / 1e9, free / 1e9))
                r = self._download(z["url"], part, z["size"])
                if r in ("paused", "cancel"):
                    if r == "cancel":
                        self._cleanup_part(tag)
                        self.task = None
                    else:
                        self._set_task(phase="paused", msg="Пауза")
                    return
                self._set_task(phase="verify", msg="Проверяем целостность…", pct=100)
                if os.path.getsize(part) != z["size"]:
                    raise RuntimeError("Размер файла не совпал, попробуй ещё раз")
                expected = ""
                if z["sha_url"]:
                    try:
                        expected = requests.get(z["sha_url"], headers=UA, timeout=15).text.split()[0].lower()
                    except Exception:
                        expected = ""
                expected = expected or (z["digest"] or "").lower()
                if expected:
                    h = hashlib.sha256()
                    done = 0
                    with open(part, "rb") as f:
                        while True:
                            b = f.read(8 << 20)
                            if not b:
                                break
                            h.update(b)
                            done += len(b)
                            self._set_task(pct=100.0 * done / z["size"], msg="Проверяем SHA-256…")
                    if h.hexdigest().lower() != expected:
                        os.remove(part)
                        raise RuntimeError("Контрольная сумма не совпала — файл скачан с ошибкой. Нажми «Повторить»: он скачается заново.")
                os.replace(part, final)
            self._extract(final, tag)
            self._activate(tag)
            try:
                os.remove(final)
            except OSError:
                pass
            self._set_task(phase="done", pct=100, msg="Готово")
        except Exception as e:
            self._set_task(phase="error", error=str(e), msg=str(e))

    def _download(self, url, dest, total):
        attempt = 0
        samples = collections.deque(maxlen=40)
        while True:
            have = os.path.getsize(dest) if os.path.exists(dest) else 0
            if have > total:
                os.remove(dest)
                have = 0
            if have == total:
                return "ok"
            try:
                h = dict(UA)
                if have:
                    h["Range"] = "bytes=%d-" % have
                with requests.get(url, headers=h, stream=True, timeout=(15, 30)) as r:
                    if r.status_code == 416:
                        os.remove(dest)
                        continue
                    if r.status_code not in (200, 206):
                        raise IOError("HTTP %d" % r.status_code)
                    mode = "ab" if r.status_code == 206 else "wb"
                    if mode == "wb":
                        have = 0
                    with open(dest, mode) as f:
                        for chunk in r.iter_content(1 << 20):
                            if self.stop_flag:
                                return self.stop_flag
                            f.write(chunk)
                            have += len(chunk)
                            now = time.time()
                            samples.append((now, have))
                            sp = 0
                            if len(samples) > 3:
                                dt = samples[-1][0] - samples[0][0]
                                sp = (samples[-1][1] - samples[0][1]) / dt if dt > 0 else 0
                            self._set_task(phase="download", bytes=have, pct=100.0 * have / total, speed=sp,
                                           eta=(total - have) / sp if sp > 1 else 0, msg="Скачиваем")
                attempt = 0
            except (requests.RequestException, IOError) as e:
                attempt += 1
                if attempt > 8:
                    raise RuntimeError("Нет связи с GitHub (%s). Нажми «Повторить» — докачка продолжится с того же места." % str(e)[:80])
                self._set_task(msg="Обрыв связи, повтор %d/8…" % attempt, speed=0)
                for _ in range(min(2 ** attempt, 20)):
                    if self.stop_flag:
                        return self.stop_flag
                    time.sleep(1)

    def _extract(self, zpath, tag):
        dest = self.build_dir(tag)
        tmp = dest + ".partial"
        for p in (tmp, dest):
            if os.path.exists(p):
                shutil.rmtree(lp(p), ignore_errors=True)
        os.makedirs(builds := self.builds_dir(), exist_ok=True)
        files = {}
        with zipfile.ZipFile(zpath) as zf:
            infos = [i for i in zf.infolist()]
            total = sum(i.file_size for i in infos) or 1
            done = 0
            base = os.path.abspath(tmp)
            for i in infos:
                if self.stop_flag == "cancel":
                    raise RuntimeError("Отменено")
                target = os.path.abspath(os.path.join(base, i.filename))
                if not target.startswith(base + os.sep):
                    continue
                if i.is_dir():
                    os.makedirs(lp(target), exist_ok=True)
                    continue
                os.makedirs(lp(os.path.dirname(target)), exist_ok=True)
                with zf.open(i) as src, open(lp(target), "wb") as out:
                    while True:
                        b = src.read(4 << 20)
                        if not b:
                            break
                        out.write(b)
                        done += len(b)
                        self._set_task(phase="extract", pct=100.0 * done / total, msg="Распаковываем", speed=0, eta=0)
                files[i.filename.replace("\\", "/")] = i.file_size
        with open(os.path.join(tmp, "files.json"), "w", encoding="utf-8") as f:
            json.dump(files, f)
        os.replace(tmp, dest)
        with open(os.path.join(dest, ".complete"), "w", encoding="utf-8") as f:
            json.dump({"tag": tag, "time": time.time()}, f)

    def _delta(self, rel, old):
        """Delta update: read the remote zip central directory via HTTP Range, reuse unchanged files from the old build
        (hardlink), fetch only changed members. Returns True on success, False if not worthwhile."""
        z = rel["zip"]
        tag = rel["tag"]
        self._set_task(phase="verify", msg="Сравниваем с установленной версией…", pct=0)
        zf = zipfile.ZipFile(RangeFile(z["url"], z["size"]))
        infos = [i for i in zf.infolist() if not i.is_dir()]
        oldroot = self.build_dir(old)
        reuse, fetch = [], []
        for n, i in enumerate(infos):
            if self.stop_flag == "cancel":
                raise _Cancel()
            op = os.path.join(oldroot, *i.filename.split("/"))
            try:
                if os.path.getsize(lp(op)) == i.file_size and file_crc(lp(op)) == i.CRC:
                    reuse.append((i, op))
                    continue
            except OSError:
                pass
            fetch.append(i)
            self._set_task(pct=100.0 * n / max(1, len(infos)))
        need = sum(i.compress_size for i in fetch)
        if need > 0.7 * z["size"]:
            return False
        free = shutil.disk_usage(self.root()).free
        if free < need * 2 + 2 ** 28:
            return False
        dest = self.build_dir(tag)
        tmp = dest + ".partial"
        for p in (tmp, dest):
            if os.path.exists(p):
                shutil.rmtree(lp(p), ignore_errors=True)
        base = os.path.abspath(tmp)
        os.makedirs(lp(base), exist_ok=True)
        files = {}
        for i, op in reuse:
            target = os.path.abspath(os.path.join(base, *i.filename.split("/")))
            os.makedirs(lp(os.path.dirname(target)), exist_ok=True)
            try:
                os.link(lp(op), lp(target))
            except OSError:
                shutil.copy2(lp(op), lp(target))
            files[i.filename] = i.file_size
        done = 0
        samples = collections.deque(maxlen=40)
        total = max(1, need)
        for i in fetch:
            target = os.path.abspath(os.path.join(base, *i.filename.split("/")))
            if not target.startswith(base + os.sep):
                continue
            os.makedirs(lp(os.path.dirname(target)), exist_ok=True)
            if i.compress_size == 0:
                open(lp(target), "wb").close()
                files[i.filename] = i.file_size
                continue
            hdr = _range(z["url"], i.header_offset, i.header_offset + 29)
            nlen, elen = struct.unpack("<HH", hdr[26:30])
            start = i.header_offset + 30 + nlen + elen
            dec = zlib.decompressobj(-15) if i.compress_type == 8 else None
            crc = 0
            for attempt in range(5):
                try:
                    crc = 0
                    dec = zlib.decompressobj(-15) if i.compress_type == 8 else None
                    got = 0
                    with requests.get(z["url"], headers=dict(UA, Range="bytes=%d-%d" % (start, start + i.compress_size - 1)), stream=True, timeout=(15, 30)) as r:
                        if r.status_code != 206:
                            raise IOError("HTTP %d" % r.status_code)
                        with open(lp(target), "wb") as out:
                            for chunk in r.iter_content(1 << 20):
                                if self.stop_flag == "cancel":
                                    raise _Cancel()
                                data = dec.decompress(chunk) if dec else chunk
                                out.write(data)
                                crc = zlib.crc32(data, crc)
                                got += len(chunk)
                                now = time.time()
                                samples.append((now, done + got))
                                sp = 0
                                if len(samples) > 3 and samples[-1][0] > samples[0][0]:
                                    sp = (samples[-1][1] - samples[0][1]) / (samples[-1][0] - samples[0][0])
                                self._set_task(phase="download", bytes=done + got, total=need, pct=100.0 * (done + got) / total, speed=sp,
                                               eta=(total - done - got) / sp if sp > 1 else 0,
                                               msg="Обновляем: только изменённые файлы (%s из %s)" % (fmt_mb(need), fmt_mb(z["size"])))
                            if dec:
                                tail = dec.flush()
                                out.write(tail)
                                crc = zlib.crc32(tail, crc)
                    if (crc & 0xFFFFFFFF) != i.CRC:
                        raise IOError("CRC mismatch " + i.filename)
                    break
                except _Cancel:
                    raise
                except (requests.RequestException, IOError):
                    if attempt == 4:
                        raise
                    time.sleep(2 + attempt)
            done += i.compress_size
            files[i.filename] = i.file_size
        for i in zf.infolist():
            if i.is_dir():
                os.makedirs(lp(os.path.join(base, *i.filename.split("/"))), exist_ok=True)
        with open(os.path.join(tmp, "files.json"), "w", encoding="utf-8") as f:
            json.dump(files, f)
        os.replace(tmp, dest)
        with open(os.path.join(dest, ".complete"), "w", encoding="utf-8") as f:
            json.dump({"tag": tag, "time": time.time(), "delta_from": old, "downloaded": need}, f)
        self.delta_info = {"from": old, "downloaded": need, "full": z["size"]}
        return True

    def _activate(self, tag):
        self._set_task(phase="migrate", msg="Переносим сохранения…", pct=100)
        old = self.cfg.get("installed")
        if old and old != tag and os.path.isdir(self.saved_dir(old)):
            dst = self.saved_dir(tag)
            os.makedirs(lp(dst), exist_ok=True)
            shutil.copytree(lp(self.saved_dir(old)), lp(dst), dirs_exist_ok=True)
        if old and old != tag:
            self.cfg["previous"] = old
        self.cfg["installed"] = tag
        keep = {self.cfg.get("installed"), self.cfg.get("previous")}
        for t in self.installed_tags():
            if t not in keep:
                shutil.rmtree(lp(self.build_dir(t)), ignore_errors=True)
        self.save_cfg()

    def rollback(self):
        prev = self.cfg.get("previous")
        if not prev or not os.path.exists(os.path.join(self.build_dir(prev), ".complete")):
            return {"ok": False, "error": "Нет предыдущей версии"}
        cur = self.cfg.get("installed")
        if cur and os.path.isdir(self.saved_dir(cur)):
            shutil.copytree(lp(self.saved_dir(cur)), lp(self.saved_dir(prev)), dirs_exist_ok=True)
        self.cfg["installed"], self.cfg["previous"] = prev, cur
        self.save_cfg()
        return {"ok": True}

    def verify(self):
        tag = self.cfg.get("installed")
        if not tag:
            return {"ok": False, "msg": "Игра не установлена"}
        fj = os.path.join(self.build_dir(tag), "files.json")
        if not os.path.exists(fj):
            return {"ok": bool(self.game_exe()), "msg": "Манифест не найден, проверен только AntKitchen.exe"}
        with open(fj, "r", encoding="utf-8") as f:
            files = json.load(f)
        bad = []
        for rel, size in files.items():
            if "/Saved/" in rel or rel.endswith("/"):
                continue
            p = os.path.join(self.build_dir(tag), *rel.split("/"))
            try:
                if os.path.getsize(lp(p)) != size:
                    bad.append(rel)
            except OSError:
                bad.append(rel)
        if bad:
            return {"ok": False, "msg": "Повреждено или отсутствует файлов: %d (например %s). Нажми «Переустановить»." % (len(bad), bad[0])}
        return {"ok": True, "msg": "Все %d файлов на месте" % len(files)}

    def reinstall(self):
        tag = self.cfg.get("installed") or (self.target_release() or {}).get("tag")
        if not tag:
            return {"ok": False, "error": "Нечего переустанавливать"}
        bk = ""
        if os.path.isdir(self.saved_dir(tag)):
            bk = os.path.join(self.root(), "SavedBackup", datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
            shutil.copytree(lp(self.saved_dir(tag)), lp(bk))
        shutil.rmtree(lp(self.build_dir(tag)), ignore_errors=True)
        self.cfg["installed"] = ""
        self.save_cfg()
        res = self.install(tag)
        if res.get("ok") and bk:
            threading.Thread(target=self._restore_after, args=(tag, bk), daemon=True).start()
        return res
    def _restore_after(self, tag, bk):
        # wait for the install job to finish, then copy backed-up Saved back in
        for _ in range(60 * 60 * 3):
            time.sleep(1)
            if self.task and self.task.get("phase") == "done":
                break
            if self.task is None or self.task.get("phase") == "error":
                return
        if bk and os.path.isdir(bk):
            shutil.copytree(lp(bk), lp(self.saved_dir(tag)), dirs_exist_ok=True)

    def uninstall(self):
        if self.task:
            return {"ok": False, "error": "Идёт задача"}
        cur = self.cfg.get("installed")
        bk = ""
        if cur and os.path.isdir(self.saved_dir(cur)):
            bk = os.path.join(self.root(), "SavedBackup", datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
            shutil.copytree(lp(self.saved_dir(cur)), lp(bk))
        shutil.rmtree(lp(self.builds_dir()), ignore_errors=True)
        shutil.rmtree(lp(os.path.join(self.root(), "downloads")), ignore_errors=True)
        self.cfg.update(installed="", previous="")
        self.save_cfg()
        return {"ok": True, "backup": bk}

    # ---- prerequisites
    def install_prereqs(self):
        if not IS_WIN:
            return {"ok": False, "error": "Только для Windows"}
        if self.task:
            return {"ok": False, "error": "Идёт задача"}
        redist = os.path.join(self.build_dir(self.cfg.get("installed") or "_"), "Windows", "Engine", "Extras", "Redist", "en-us")
        self.task = {"tag": "", "phase": "prereq", "pct": 100, "msg": "Ставим библиотеки (Windows спросит разрешение)…", "error": "", "bytes": 0, "total": 0, "speed": 0, "eta": 0}

        def job():
            try:
                exe, args = os.path.join(redist, "UEPrereqSetup_x64.exe"), ["/quiet", "/norestart"]
                if not os.path.exists(exe):
                    exe, args = os.path.join(redist, "vc_redist.x64.exe"), ["/install", "/quiet", "/norestart"]
                if not os.path.exists(exe):
                    exe = os.path.join(tempfile.gettempdir(), "vc_redist.x64.exe")
                    self._set_task(msg="Скачиваем библиотеки Microsoft…")
                    r = requests.get("https://aka.ms/vs/17/release/vc_redist.x64.exe", headers=UA, timeout=120)
                    r.raise_for_status()
                    with open(exe, "wb") as fh:
                        fh.write(r.content)
                self._set_task(msg="Ставим библиотеки (Windows спросит разрешение)…")
                al = ",".join("'%s'" % a for a in args)
                ps = "$p=Start-Process -FilePath '%s' -ArgumentList %s -Verb RunAs -Wait -PassThru; exit $p.ExitCode" % (exe.replace("'", "''"), al)
                r = subprocess.run(["powershell", "-NoProfile", "-Command", ps], creationflags=CREATE_NO_WINDOW, timeout=900)
                if r.returncode in (0, 3010, 1638, 1641):  # ok / reboot needed / newer already installed
                    self.cfg["prereq_done"] = True
                    self.save_cfg()
                    self._set_task(phase="done", msg="Библиотеки установлены")
                else:
                    self._set_task(phase="error", error="Установщик вернул код %s (возможно, отказ в доступе). Попробуй ещё раз." % r.returncode)
            except Exception as e:
                self._set_task(phase="error", error="Не удалось: %s" % e)
        threading.Thread(target=job, daemon=True).start()
        return {"ok": True}

    def mark_prereq_done(self):
        self.cfg["prereq_done"] = True
        self.save_cfg()
        return True

    # ---- launch
    def build_args(self, opts):
        a = []
        if opts.get("tutorial"):
            a.append("-AntTutorial")
        if opts.get("admin"):
            a += ["-AntAdmin", "-log"]
        if opts.get("windowed"):
            a.append("-WINDOWED")
        if opts.get("args"):
            try:
                a += shlex.split(opts["args"], posix=False)
            except ValueError:
                a += opts["args"].split()
        return a

    def play(self, opts=None):
        opts = opts or self.cfg.get("opts") or {}
        self.cfg["opts"] = opts
        self.save_cfg()
        exe = self.game_exe()
        if not exe:
            return {"ok": False, "error": "Игра не установлена"}
        if not IS_WIN:
            return {"ok": False, "error": "Нативного запуска на этой ОС нет — см. вкладку Mac"}
        try:
            subprocess.Popen([exe] + self.build_args(opts), cwd=os.path.dirname(exe), creationflags=0x00000008)
            self.launching_until = time.time() + 25
            return {"ok": True}
        except Exception as e:
            return {"ok": False, "error": str(e)}

    # ---- logs
    def collect_logs(self):
        s = self.saved_dir()
        if not s or not os.path.isdir(s):
            return {"ok": False, "error": "Папки Saved ещё нет — игру ещё не запускали"}
        out = os.path.join(desktop_dir(), "AntKitchen-logs-%s.zip" % datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
        n = 0
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
            for sub in ("Logs", "Crashes", "Playtest"):
                d = os.path.join(s, sub)
                for dp, _, fns in os.walk(d):
                    for fn in fns:
                        p = os.path.join(dp, fn)
                        try:
                            zf.write(p, os.path.join(os.path.relpath(dp, s), fn))
                            n += 1
                        except OSError:
                            pass
            zf.writestr("launcher-info.txt", "launcher %s\nbuild %s\n%s\n" % (VERSION, self.cfg.get("installed"), datetime.datetime.now().isoformat()))
        if IS_WIN:
            subprocess.Popen(["explorer", "/select,", out])
        return {"ok": True, "path": out, "count": n}

    # ---- launcher self update
    def apply_self_update(self):
        u = self.selfupd
        if not getattr(sys, "frozen", False):
            return {"ok": False, "error": "Самообновление работает только в собранном exe"}
        if not u.get("url"):
            return {"ok": False, "error": "Нет обновления"}
        try:
            cur = sys.executable
            new = os.path.join(tempfile.gettempdir(), "AntKitchenLauncher.new.exe")
            with requests.get(u["url"], headers=UA, stream=True, timeout=(15, 60)) as r:
                r.raise_for_status()
                with open(new, "wb") as f:
                    for c in r.iter_content(1 << 20):
                        f.write(c)
            bat = os.path.join(tempfile.gettempdir(), "ak_update.bat")
            with open(bat, "w", encoding="ascii") as f:
                f.write('@echo off\r\nset n=0\r\n:retry\r\nping 127.0.0.1 -n 3 >nul\r\nmove /y "%s" "%s" >nul 2>&1\r\n'
                        'if errorlevel 1 (set /a n+=1 & if %%n%% LSS 20 goto retry)\r\nstart "" "%s"\r\ndel "%%~f0"\r\n' % (new, cur, cur))
            subprocess.Popen(["cmd", "/c", bat], creationflags=CREATE_NO_WINDOW | 0x00000008, close_fds=True)
            threading.Timer(0.5, lambda: os._exit(0)).start()
            return {"ok": True}
        except Exception as e:
            return {"ok": False, "error": str(e)}

    def install_webview2(self):
        try:
            p = os.path.join(tempfile.gettempdir(), "MicrosoftEdgeWebview2Setup.exe")
            r = requests.get("https://go.microsoft.com/fwlink/p/?LinkId=2124703", headers=UA, timeout=60)
            with open(p, "wb") as f:
                f.write(r.content)
            subprocess.run([p, "/silent", "/install"], timeout=300)
            return True
        except Exception:
            return False


PUBLIC = ["get_state", "set_opts", "set_platform", "set_chat_url", "set_channel", "choose_root", "open_url", "open_path", "copy_text",
          "start_steam", "install", "pause", "cancel", "dismiss", "rollback", "verify", "reinstall", "uninstall", "install_prereqs",
          "mark_prereq_done", "play", "collect_logs", "apply_self_update", "refresh"]


class Api:
    """Thin facade: pywebview walks public attributes, so keep state private inside _c."""
    def __init__(self):
        self._c = Core()


def _mk(n):
    def m(self, *a):
        return getattr(self._c, n)(*a)
    m.__name__ = n
    return m


for _n in PUBLIC:
    setattr(Api, _n, _mk(_n))


def grab_window(title, out):
    """PrintWindow screenshot of our own window (works while another app is on top). Test helper."""
    import ctypes
    from ctypes import wintypes
    from PIL import Image
    u, g = ctypes.windll.user32, ctypes.windll.gdi32
    hwnd = u.FindWindowW(None, title)
    rc = wintypes.RECT()
    u.GetWindowRect(hwnd, ctypes.byref(rc))
    w, h = rc.right - rc.left, rc.bottom - rc.top
    hdc = u.GetWindowDC(hwnd)
    mdc = g.CreateCompatibleDC(hdc)
    bmp = g.CreateCompatibleBitmap(hdc, w, h)
    g.SelectObject(mdc, bmp)
    u.PrintWindow(hwnd, mdc, 2)

    class BMI(ctypes.Structure):
        _fields_ = [("biSize", wintypes.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long), ("biPlanes", wintypes.WORD),
                    ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD), ("biSizeImage", wintypes.DWORD),
                    ("x", ctypes.c_long), ("y", ctypes.c_long), ("c", wintypes.DWORD), ("i", wintypes.DWORD)]
    bi = BMI(ctypes.sizeof(BMI), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
    buf = ctypes.create_string_buffer(w * h * 4)
    g.GetDIBits(mdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
    Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("RGB").save(out)
    g.DeleteObject(bmp); g.DeleteDC(mdc); u.ReleaseDC(hwnd, hdc)


def selftest(core, logp):
    """Dev-only end-to-end check (AK_AUTO=1): install -> migrate Saved -> launch headless -> verify -> kill test game."""
    def log(m):
        with open(logp, "a", encoding="utf-8") as fh:
            fh.write("%s %s\n" % (time.strftime("%H:%M:%S"), m))
    try:
        time.sleep(4)
        while core.rel_status == "loading":
            time.sleep(1)
        log("releases: %s" % [r["tag"] for r in core.releases])
        log("target: %s" % (core.target_release() or {}).get("tag"))
        # fake previous build with a Saved folder to test migration
        tgt = (core.target_release() or {}).get("tag")
        if tgt in core.installed_tags():
            core.cfg["installed"] = tgt
            log("target already installed, skipping download")
            old = None
        else:
            old = core.build_dir("playtest-0000-fake")
        if old:
            os.makedirs(os.path.join(old, "Windows", "AntKitchen", "Saved", "SaveGames"), exist_ok=True)
            with open(os.path.join(old, "Windows", "AntKitchen", "Saved", "SaveGames", "colony.sav"), "w") as fh:
                fh.write("fake-colony")
            with open(os.path.join(old, ".complete"), "w") as fh:
                fh.write("{}")
            open(os.path.join(old, "Windows", "AntKitchen.exe"), "w").close()
            core.cfg["installed"] = "playtest-0000-fake"
            r = core.install()
            log("install start: %s" % r)
        last = 0
        while old:
            time.sleep(0.5)
            t = core.task
            if not t:
                log("task cleared by UI (done)")
                break
            if time.time() - last > 10:
                log("phase=%s pct=%.1f speed=%.1fMB/s eta=%s msg=%s" % (t.get("phase"), t.get("pct", 0), t.get("speed", 0) / 1e6, int(t.get("eta", 0)), t.get("msg")))
                last = time.time()
            if t.get("phase") in ("done", "error"):
                log("final: %s %s" % (t.get("phase"), t.get("error")))
                break
        tag = core.cfg.get("installed")
        log("installed=%s previous=%s exe=%s" % (tag, core.cfg.get("previous"), core.game_exe()))
        mig = os.path.join(core.saved_dir(), "SaveGames", "colony.sav")
        log("migrated colony.sav exists: %s" % os.path.exists(mig))
        log("verify: %s" % core.verify())
        log("steam: %s  vcrt: %s prereq: %s" % (steam_status(), vcrt_ok(), core.get_state()["prereq"]))
        res = core.play({"tutorial": False, "admin": False, "windowed": False, "args": "-RenderOffScreen -nosound -log"})
        log("play: %s" % res)
        alive = []
        for _ in range(40):
            time.sleep(2)
            alive = [p for p in list_procs(("antkitchen",)) if core.builds_dir().lower() in (p[2] or "").lower()]
            if alive and core.get_state()["running"]:
                break
        log("running procs: %s ; state.running=%s" % (alive, core.get_state()["running"]))
        time.sleep(15)
        logs = os.path.join(core.saved_dir(), "Logs")
        log("logs dir: %s" % (os.listdir(logs) if os.path.isdir(logs) else "missing"))
        alive = [p for p in list_procs(("antkitchen",)) if core.builds_dir().lower() in (p[2] or "").lower()]
        log("still alive: %s" % alive)
        for pid, _, path in alive:
            subprocess.run(["taskkill", "/PID", str(pid), "/F"], creationflags=CREATE_NO_WINDOW)
        time.sleep(2)
        log("after kill running=%s" % core.get_state()["running"])
        log("collect_logs: %s" % core.collect_logs())
        log("SELFTEST DONE")
    except Exception as e:
        import traceback
        log("EXC " + traceback.format_exc())


def ensure_webview2():
    if webview2_ok():
        return True
    import ctypes
    r = ctypes.windll.user32.MessageBoxW(0, "Для окна лаунчера нужен Microsoft Edge WebView2 Runtime. Скачать и установить сейчас (около 2 МБ)?", "Ant Kitchen Launcher", 0x24)
    if r == 6:
        Core.install_webview2(None)
    return webview2_ok()


def main():
    import webview
    if IS_WIN and not ensure_webview2():
        return
    api = Api()
    win = webview.create_window("Ant Kitchen Launcher", url=resource("web/index.html"), js_api=api, width=1180, height=780,
                                min_size=(1000, 680), background_color="#1B120B")
    api._c.window = win
    if os.environ.get("AK_AUTO"):
        threading.Thread(target=selftest, args=(api._c, os.environ["AK_AUTO"]), daemon=True).start()
    shot = os.environ.get("AK_SHOT")
    if shot:
        def snap():
            time.sleep(float(os.environ.get("AK_SHOT_DELAY", "9")))
            try:
                tabs = [x for x in os.environ.get("AK_SHOT_TABS", "play").split(",") if x]
                for tb in tabs:
                    win.evaluate_js("window.go && go('%s')" % tb)
                    time.sleep(1.5)
                    grab_window("Ant Kitchen Launcher", shot.replace(".png", "_%s.png" % tb))
                dump = os.environ.get("AK_DUMP")
                if dump:
                    txt = win.evaluate_js("document.querySelector('#tab-play').innerText")
                    with open(dump, "w", encoding="utf-8") as fh:
                        fh.write(str(txt))
            except Exception as e:
                with open(shot + ".err", "w") as fh:
                    fh.write(repr(e))
            finally:
                if os.environ.get("AK_SHOT_EXIT"):
                    os._exit(0)
        threading.Thread(target=snap, daemon=True).start()
    webview.start(gui="edgechromium" if IS_WIN else None, debug=bool(os.environ.get("AK_DEBUG")),
                  storage_path=os.path.join(cfg_dir(), "webview"), private_mode=False)


if __name__ == "__main__":
    main()
