# Captured frames + audio.wav (Tools/capture rig) -> the sound demo mp4 with captions of what the music is doing.
# Usage: python demo_post.py <take dir> <out.mp4>
import sys, os, subprocess, io
D, OUT = sys.argv[1], sys.argv[2]
LABELS = [
    (0.0, 8.2, "Рейд: спокойный грув (слой Base)"),
    (8.4, 16.2, "Человек входит и злится - слой Tension"),
    (16.4, 23.2, "Погоня, топот, тапок - слой Action"),
    (23.4, 33.6, "Хаос на кухне: грохот по материалу"),
    (33.8, 49.6, "Всё стихает - музыка возвращается к груву"),
    (49.8, 65.6, "Последний свет: 30 секунд - слой Urgency"),
    (65.8, 80.0, "Эвакуация: победный стинг и марш гнезда"),
]
font = "C\\:/Windows/Fonts/arialbd.ttf"
tmp = os.environ.get("TEMP", "C:/Windows/Temp").replace("\\", "/")
def textfile(i, s):
    p = "%s/ak_sound_lbl%d.txt" % (tmp, i)
    io.open(p, "w", encoding="utf-8").write(s)
    return p.replace(":", "\\:")   # the label goes through a file: its colons and commas need no escaping
parts = []
for i, (a, b, txt) in enumerate(LABELS):
    parts.append("drawtext=fontfile='%s':textfile='%s':x=40:y=40:fontsize=34:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=14:enable='between(t,%.2f,%.2f)'" % (font, textfile(i, txt), a, b))
parts.append("drawtext=fontfile='%s':textfile='%s':x=w-tw-30:y=h-th-26:fontsize=22:fontcolor=white@0.8" % (font, textfile(99, "Ant Kitchen - звук и музыка (code/ANT-sound)")))
script = os.path.join(D, "post_filter.txt")
io.open(script, "w", encoding="utf-8").write("[0:v]" + ",".join(parts) + "[v]")
os.makedirs(os.path.dirname(OUT), exist_ok=True)
cmd = ["ffmpeg", "-v", "error", "-y", "-framerate", "30", "-i", os.path.join(D, "f%05d.png"), "-i", os.path.join(D, "audio.wav"),
       "-filter_complex_script", script, "-map", "[v]", "-map", "1:a", "-c:v", "libx264", "-preset", "medium", "-crf", "19",
       "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", OUT]
subprocess.run(cmd, check=True)
print("wrote", OUT, os.path.getsize(OUT) // 1024, "KB")
