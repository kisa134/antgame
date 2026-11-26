# Dev helper: python grep_log.py <log> <out> <regex>  -> the matching lines (UTF-8), the timestamp prefix trimmed.
import re, sys
src, out, pat = sys.argv[1], sys.argv[2], re.compile(sys.argv[3])
with open(src, encoding='utf-8', errors='replace') as f, open(out, 'w', encoding='utf-8') as o:
    for ln in f:
        if pat.search(ln):
            o.write(re.sub(r'^\[[^\]]*\]\[[^\]]*\]', '', ln.rstrip())[:400] + '\n')
