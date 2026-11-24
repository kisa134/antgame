#!/usr/bin/env python3
"""Append a UTF-8 text fragment to a project doc that is UTF-8 with BOM + CRLF, keeping both.  append_doc.py <doc> <fragment>"""
import sys
doc, frag = sys.argv[1], sys.argv[2]
b = open(doc, "rb").read()
t = open(frag, encoding="utf-8-sig").read().replace("\r\n", "\n").replace("\n", "\r\n")
if not b.endswith(b"\r\n"):
    b += b"\r\n"
open(doc, "wb").write(b + t.encode("utf-8"))
print("appended", len(t), "chars to", doc)
