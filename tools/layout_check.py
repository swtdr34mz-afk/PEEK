#!/usr/bin/env python3
"""Checks that every control of the panel sits inside its section's box (PEEK_DUMP=1 PeekShot ... | this)."""
import re, sys
src = open(sys.argv[1]).read()
X = lambda px: 38 + (px - 45) * 1.797
Y = lambda py: (py - 95) * 1.833
secs = [(X(float(a)), Y(float(b)), X(float(c)), Y(float(d)), t) for a, b, c, d, t in
        re.findall(r'\{([\d.]+), ([\d.]+), ([\d.]+), ([\d.]+), "([A-Z0-9 ]+)", (?:true|false)\}', src)]
TITLE, M = 24, 3          # (a section's heading: its top 24 px; a margin of 3 px all round)
bad = 0
for line in sys.stdin:
    if not line.startswith("C "):
        continue
    f = line.split()
    x0, y0, x1, y1 = map(int, f[1:5])
    kind = f[5] if len(f) > 5 else ""
    if "Leds" in kind:                             # (a row of LEDs: its bounds pad the pills, 21 x 9)
        x0, x1, y0, y1 = x0 + 17, x1 - 17, y0 + 14, y1 - 14
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    home = [s for s in secs if s[0] <= cx <= s[2] and s[1] <= cy <= s[3]]
    if not home:
        continue                                   # (the bottom strip)
    sx0, sy0, sx1, sy1, t = home[0]
    tw = 16 + 12 * len(t)                          # (the heading's width: only there must nothing come up so high)
    top = sy0 + TITLE if x0 < sx0 + tw else sy0 + 8
    if x0 < sx0 + M or x1 > sx1 - M or y0 < top or y1 > sy1 - M:
        bad += 1
        print(f"{t:14s} control {x0},{y0}-{x1},{y1} outside {sx0:.0f},{sy0 + TITLE:.0f}-{sx1:.0f},{sy1:.0f}")
print("controls outside their box:", bad)
