#!/usr/bin/env python3
"""Synthetic PlaytestKit sessions in the exact on-disk format (for testing ptk_report.py without a game build).
   python make_sample_data.py <out_folder> [n_sessions]"""
import json, os, random, sys, datetime as dt

out = sys.argv[1]; n = int(sys.argv[2]) if len(sys.argv) > 2 else 6
random.seed(7)
STEPS = [("move", 20), ("sprint", 25), ("pickup", 40), ("carry", 60), ("trail", 50), ("raid", 120)]
CAUSES = ["human", "spray", "fall", "drown", "trap", "human", "human"]
HOT = [(-3000, 1500), (2500, -2000), (0, 3500)]


def pos(raid):
    if random.random() < .5:
        cx, cy = random.choice(HOT)
        return cx + random.gauss(0, 500), cy + random.gauss(0, 400)
    return random.uniform(-6500, 6500), random.uniform(-4500, 4500)


for si in range(n):
    start = dt.datetime(2026, 11, 24, 12, 0, 0, tzinfo=dt.timezone.utc) + dt.timedelta(hours=si)
    sid = start.strftime("%Y%m%d-%H%M%S") + f"-{si:04x}"
    d = os.path.join(out, sid); os.makedirs(os.path.join(d, "shots"), exist_ok=True)
    seq = 0; t = 0; lines = []

    def E(name, cat, props, p=None, tms=None, raid=None):
        global seq
        seq += 1
        ctx = {"map": "Entry", "role": "host" if si % 2 == 0 else "client", "mode": "raid" if raid else "tutorial"}
        if raid: ctx["raid_no"] = raid
        if p: ctx["pos"] = {"x": round(p[0]), "y": round(p[1]), "z": 0}
        lines.append({"seq": seq, "t": int(tms if tms is not None else t), "ts": (start + dt.timedelta(milliseconds=int(tms if tms is not None else t))).strftime("%Y-%m-%dT%H:%M:%S.000Z"), "name": name, "cat": cat, "props": props, "ctx": ctx})

    E("session_start", "session", {"kit_version": "1.0", "machine": {"os": "Win11", "cpu": "Ryzen 5", "cores": 6, "gpu": "RTX 3060", "ram_gb": 16, "engine": "5.8"}})
    E("level_start", "level", {"map": "Entry", "load_ms": 3000 + random.randint(0, 3000)})
    for i, (sid_, par) in enumerate(STEPS):
        if si == 3 and i >= 4: break  # one tester quits mid-tutorial
        E("tutorial_step_start", "progress", {"id": sid_, "no": i, "par": par}); t += 1000
        sec = par * random.uniform(.7, 2.4 if sid_ == "trail" else 1.5); t += int(sec * 1000)
        E("tutorial_step_complete", "progress", {"id": sid_, "no": i, "par": par, "seconds": round(sec, 1), "retries": random.randint(0, 2), "assists": random.randint(0, 1)})
    else:
        E("tutorial_finish", "progress", {"id": "tutorial", "seconds": round(t / 1000, 1)})
    if si != 3:
        for raid in range(1, 4):
            E("raid_start", "raid", {"raid_no": raid, "tier": 1}, raid=raid)
            for k in range(60):
                t += 2000; E("breadcrumb", "breadcrumb", {"v": 250}, pos(raid), raid=raid)
                if k % 2 == 0:
                    E("perf_sample", "perf", {"fps_avg": random.gauss(70 - raid * 3, 8), "fps_min": 30, "fps_p1": random.gauss(48, 6), "frame_ms": 14, "gpu_ms": 8, "game_ms": 6, "render_ms": 7, "mem_mb": 5200, "hitches": random.randint(0, 2)}, raid=raid)
                if k % 3 == 0 and si % 2 == 1:
                    E("net_sample", "net", {"role": "client", "conns": 1, "ping_avg": random.gauss(45, 10), "ping_max": 90, "loss_in_pct": abs(random.gauss(.5, 1)), "loss_out_pct": 0, "in_kbps": 40, "out_kbps": 12}, raid=raid)
                if random.random() < .05: E("hitch", "hitch", {"ms": random.randint(110, 400), "loading": False}, pos(raid), raid=raid)
                if random.random() < .12: E("carry_event", "carry", {"kind": "Delivered", "loot": "crumb", "value": 5}, pos(raid), raid=raid); E("economy_source", "economy", {"item": "crumb", "amount": 5, "src": "carry_delivered"}, raid=raid)
                if random.random() < .03 * raid: E("death", "combat", {"cause": random.choice(CAUSES), "life_seconds": random.randint(20, 200), "killer": "HumanCharacter"}, pos(raid), raid=raid)
            E("raid_end", "raid", {"raid_no": raid, "tier": 1, "end_name": random.choice(["left", "left", "timeout", "wiped"])}, raid=raid)
    for tag, text in random.sample([("bug", "муравей застрял в тостере"), ("confusing", "не понял куда нести еду"), ("fun", "цепочка из шести муравьёв через раковину"), ("boring", "долго идти от гнезда")], 3):
        t += 5000; shot = f"m{seq + 1}.jpg"
        E("marker", "marker", {"tag": tag, "text": text, "shot": shot, "perf30": [{"t": 0, "fps": 60 + random.randint(-20, 5)}], "recent": ["carry_event", "death"]}, pos(1), raid=1)
    if si == 2: E("crash", "crash", {"fatal": True, "msg": "Assertion failed: Ant != nullptr [AntCharacter.cpp] 0x7ff", "recent": []})
    E("error", "error", {"log_cat": "LogTemp", "msg": "Accessed None trying to read property X"})
    if si != 3 and si != 2:
        for q, ty, v in [("overall", "scale", random.randint(5, 9)), ("play_again", "choice", random.choice(["да", "да", "нет"])), ("difficulty", "scale", random.randint(2, 5)),
                         ("best_moment", "text", "когда вся колония донесла торт"), ("confusing", "text", "непонятно как ставить тропу")]:
            E("survey_answer", "survey", {"survey_id": "post", "qid": q, "type": ty, "value": v})
    with open(os.path.join(d, "events.jsonl"), "w", encoding="utf-8") as f:
        f.write("\n".join(json.dumps(x, ensure_ascii=False) for x in lines) + "\n")
    json.dump({"schema_v": 1, "project": "ant-kitchen", "build": {"version": "0.9.1", "git_sha": "abc123", "platform": "Win64", "config": "Development"}, "session_id": sid,
               "player": {"anon_id": f"anon{si % 4:02d}", "role": "host" if si % 2 == 0 else "client"}, "started": start.isoformat(), "ended": None if si == 2 else (start + dt.timedelta(milliseconds=t)).isoformat(),
               "counters": {"events": seq, "errors": 1, "warnings": 4}}, open(os.path.join(d, "session.json"), "w"))
print("wrote", n, "sessions to", out)
