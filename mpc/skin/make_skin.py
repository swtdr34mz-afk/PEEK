#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 swtdr34mz (PEEK)
"""The MPC skin (Plugin Skins/: TUI.json, Q-Links.json and the pictures) from the renderer's output.

    python3 mpc/skin/make_skin.py <render folder> mpc/plugin/skins

The renderer (render_skin.cpp) draws each MPC page with the VST3's own art and cuts every control out at every value;
this places each picture where it was cut from, bound to the MPC plugin's parameter:

  knobs      film strips (a frame a value), dragged; a double tap opens the MPC's editor (its list for a choice)
  buttons    a picture for each value; a tap switches it (ANIMATE 1 and 2: on while held)
  LED lists  each row a radio button

The pages: four function keys, as the Peak's panel: OSC, FILTER, ENV, SCREEN. The controls carry their MPC handles
(the renderer's manifest says them) and when they are shown (one of two parameters on one knob; the display's pages).
On each page the 16 Q-Links: each row of four Q-Links four knobs the renderer named. Standard library only (Pillow, if
there, makes the pictures smaller).
"""
import json
import os
import shutil
import sys

# The MPC plugin's parameters (mpc/plugin/src/PeekMpc.cpp): 0 VOLUME, then the patch's 275, then PEEK's own
NP = 275
GLOBAL = {"volume": 0}
for _i, _id in enumerate(["voices", "quality", "anim1", "anim2", "hold", "target", "vdec", "vinc", "init", "audition",
                         "prog", "pdec", "pinc", "rvtype"]):
    GLOBAL[_id] = 1 + NP + _i
# Program (Track) mode: the knobs as the MPC's own program knobs, bottom left Q-Link 1
PROGRAM_QLINKS = ["ch_level", "dl_level", "rv_level", "volume",
                  "aenv_att", "aenv_dec", "aenv_sus", "aenv_rel",
                  "m1env_att", "m1env_dec", "m1env_sus", "m1env_rel",
                  "freq", "reso", "drive", "fenv1"]


class O(list):
    """A JSON object kept in order, with repeated keys (the overlays' two "version"s)."""
    def __init__(self, *pairs):
        super().__init__(pairs)


def dump(v, ind=0):
    sp = "  " * ind
    if isinstance(v, O):
        if not v:
            return "{}"
        return "{\n" + ",\n".join("%s  %s: %s" % (sp, json.dumps(k), dump(x, ind + 1)) for k, x in v) + "\n" + sp + "}"
    if isinstance(v, list):
        if not v:
            return "[]"
        if all(not isinstance(x, (O, list)) for x in v):
            return "[" + ", ".join(json.dumps(x) for x in v) + "]"
        return "[\n" + ",\n".join(sp + "  " + dump(x, ind + 1) for x in v) + "\n" + sp + "]"
    return json.dumps(v)


def box(r):
    return "%d %d %d %d" % tuple(r)


def image_data(img):
    return O(("version", 2), ("imageType", "Regular"), ("colour", "0"), ("image", img))


def bounds(r, focus="No", hide_handles=None, show="Show"):
    if hide_handles:
        return O(("version", 2), ("acceptsHWFocus", focus), ("showWhenDataModelInvalid", show), ("whenVisible", "Always"),
                 ("boundsType", "Absolute"), ("bounds", box(r)), ("additionalInvalidatingHandles", hide_handles))
    return O(("version", 1), ("acceptsHWFocus", focus), ("showWhenDataModelInvalid", show), ("whenVisible", "Always"),
             ("boundsType", "Absolute"), ("bounds", box(r)))


def entry(name, typ, data, b, remap=None):
    return O(("version", 2),
             ("componentData", O(("version", 1), ("name", name), ("type", typ), ("data", data))),
             ("handle remapping", O(("version", 1), ("map", [O(("key", k), ("value", v)) for k, v in (remap or [])]))),
             ("bounds", b))


def action(on, handler, handle, extra):
    return O(("version", 1), ("onAction", on), ("handler", handler), ("handleName", handle), ("additionalData", extra))


def component(actions, children, ignore_mouse=False):
    clear = O(("version", 1), ("colour", "0"), ("image", ""))
    return O(("version", 2), ("actions", actions),
             ("backgroundData", O(("version", 1), ("focussed", clear), ("unfocussed", clear))),
             ("ignoreMousePresses", ignore_mouse), ("disableCoarseDataWheel", False), ("componentsData", children))


FOCUS_LOST = O(("version", 1), ("onAction", "Focus Lost"), ("handler", "Focus Lost"), ("handleName", "Focus Lost"),
               ("additionalData", ""))


def overlay(typ, data):
    return component([], [entry("overlay", typ, data, bounds((0, 0, 1280, 800), "Yes"))], ignore_mouse=True)


def range_handle(a, b, steps, h):
    return ("IndexedEnabling/%d/%d/%s" % (a, steps, h)) if a == b else ("RangedEnabling/%d/%d/%d/%s" % (a, b, steps, h))


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    src, out = argv[1], argv[2]
    m = json.load(open(os.path.join(src, "manifest.json")))
    ids = m["params"]
    if len(ids) != NP:
        sys.exit("the renderer knows %d parameters, the MPC plugin %d" % (len(ids), NP))

    def mpc(pid):
        if pid in GLOBAL:
            return GLOBAL[pid]
        if pid not in ids:
            sys.exit("%s is not one of the MPC plugin's parameters" % pid)
        return 1 + ids.index(pid)

    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    used = set()

    def use(f):
        used.add(f)
        return f

    def knob_overlay(bipolar):
        return overlay("KnobOverlay", O(("version", 2), ("version", 1), ("actions", [FOCUS_LOST]), ("overlayType", "Knob With Header"),
                                        ("scaleType", "Linear"), ("polarityType", "Bipolar" if bipolar else "Unipolar"),
                                        ("showMinusSign", True), ("handleName", "Data")))
    defs = [("knob overlay", knob_overlay(False)), ("knob overlay bipolar", knob_overlay(True)),
            ("menu overlay", overlay("MenuOverlay", O(("version", 1), ("version", 1), ("actions", [FOCUS_LOST]),
                                                      ("withNumbers", False), ("handleName", "Data"))))]
    defined = set()
    focus = O(("version", 1), ("backgroundColour", "19ffffff"), ("outlineColour", "ffe81f40"), ("backgroundInset", 1.0),
              ("outlineThickness", 1.0))
    names = {}                           # the discrete parameters' names by MPC index (replacementStrings)

    def tab_children(page):
        kids = [entry("panel", "Image", image_data(use(page["background"])), bounds((0, 0, 1280, 628)))]
        boxes = []

        for i, c in enumerate(page["controls"]):
            h, show = c["handle"], list(c["show"])
            if c["names"] and h.startswith("Parameter "):
                names[int(h.split()[1])] = c["names"]
            hb = lambda extra=None: bounds(c["rect"], "No", show + (extra or []) or None, "Hide")
            if c["kind"] == "knob":
                kx, ky, w, hh = c["rect"]
                ov = "menu overlay" if c["names"] else "knob overlay bipolar" if c["bipolar"] else "knob overlay"
                name = "%s knob %d %s" % (page["name"].lower(), i, c["id"])
                knob = O(("version", 3), ("knobType", "FilmStrip"), ("filmStrip", use(c["strip"])), ("numFrames", c["frames"]),
                         ("invert", False), ("dragOrientation", "Vertical"), ("handleName", "Data"))
                defs.append((name, component(
                    [action("Mouse Down", "Q-Link", "Data", ""), action("Double Click", "Show Overlay", "Data", ov),
                     action("Enter Pressed", "Show Overlay", "Data", ov)],
                    [entry("knob", "Knob", knob, bounds((0, 0, w, hh))),
                     entry("focus", "Focus", focus, O(("version", 1), ("acceptsHWFocus", "No"), ("showWhenDataModelInvalid", "Show"),
                                                      ("whenVisible", "WhenFocussed"), ("boundsType", "Absolute"),
                                                      ("bounds", box((0, 0, w, hh)))))])))
                kids.append(entry(c["id"], name, O(("version", 1), ("handleName", "Data")),
                                  bounds(c["rect"], "Yes", show or None, "Hide"), [("Data", h)]))
                boxes.append((c["id"], c["rect"]))
            elif c["kind"] == "button":
                mode, steps = c["mode"], c["steps"]
                if mode == "cycle":      # a picture a value, shown while the value is its own; a press: the next
                    for v, nx in enumerate(c["next"]):
                        data = O(("version", 2), ("onImage", use(c["off"])), ("offImage", c["off"]), ("buttonId", nx),
                                 ("numButtonsInGroup", steps), ("handleName", h), ("gestureBehaviour", "Instant"))
                        kids.append(entry(c["id"], "Button", data, hb(["IndexedEnabling/%d/%d/%s" % (v, steps, h)])))
                else:
                    if mode == "momentary":
                        hh, bid, n, g = "ResetOnGestureEnd/" + h, 1, 1, "Hold"
                    elif mode == "pulse":        # (a key that sets its value while held: VALUE - / + of one row)
                        hh, bid, n, g = "ResetOnGestureEnd/" + h, c["set"], steps, "Hold"
                    elif mode == "set":
                        hh, bid, n, g = h, c["set"], steps, "Instant"
                    else:
                        hh, bid, n, g = h, 1, 1, "Instant"
                    data = O(("version", 2), ("onImage", use(c["on"])), ("offImage", use(c["off"])), ("buttonId", bid),
                             ("numButtonsInGroup", n), ("handleName", hh), ("gestureBehaviour", g))
                    kids.append(entry(c["id"] or "local", "Button", data, hb()))
                boxes.append((c["id"], c["rect"]))
            elif c["kind"] == "leds":
                for row in c["rows"]:
                    data = O(("version", 2), ("onImage", use(row["on"])), ("offImage", use(row["off"])), ("buttonId", row["value"]),
                             ("numButtonsInGroup", c["steps"]), ("handleName", h), ("gestureBehaviour", "Instant"))
                    kids.append(entry(c["id"], "Button", data, bounds(row["rect"], "No", show or None, "Hide")))
                    boxes.append((c["id"], row["rect"]))
            elif c["kind"] == "image":
                kids.append(entry("display", "Image", image_data(use(c["image"])), hb()))
            else:                        # the display's values: the MPC's own text
                style = O(("version", 1), ("font", O(("version", 1), ("name", "Roboto"), ("style", "Regular"),
                                                     ("height", round(c["height"], 1)))),
                          ("colour", "ffd4e7fb"),
                          ("justification", ("right" if c["right"] else "left") + " verticallyCentred"), ("case", "Original"))
                data = O(("version", 1), ("textStyle", style), ("type", "Value"), ("handleName", h))
                kids.append(entry(c["id"] + " value", "Label", data, hb()))
        return kids, boxes

    def frame(group, boxes):
        """The Q-Link group's frame: around its controls, and around any other it would cut (with an even gap)."""
        mine = [r for p, r in boxes if p in group]
        if not mine:
            return None
        x0, y0 = min(r[0] for r in mine), min(r[1] for r in mine)
        x1, y1 = max(r[0] + r[2] for r in mine), max(r[1] + r[3] for r in mine)
        grown = True
        while grown:
            grown = False
            for _p, r in boxes:
                a, b, c, d = x0 - 6, y0 - 6, x1 + 6, y1 + 6
                ix = min(c, r[0] + r[2]) - max(a, r[0])
                iy = min(d, r[1] + r[3]) - max(b, r[1])
                inside = r[0] >= a and r[1] >= b and r[0] + r[2] <= c and r[1] + r[3] <= d
                if ix > 0 and iy > 0 and not inside:
                    x0, y0, x1, y1 = min(x0, r[0]), min(y0, r[1]), max(x1, r[0] + r[2]), max(y1, r[1] + r[3])
                    grown = True
        a, b = max(0, x0 - 6), max(0, y0 - 6)
        return (a, b, min(1280, x1 + 6) - a, min(628, y1 + 6) - b)

    tabs, qmaps = [], []
    for page in m["pages"]:
        kids, boxes = tab_children(page)
        rows = [[None] * 4 for _ in range(4)]
        for c in page["controls"]:
            if c.get("q", -1) >= 0 and c["kind"] == "knob":
                rows[c["q"] // 4][c["q"] % 4] = c["id"]
        rects, q = [], {}
        for row, ids_ in enumerate(rows):
            got = [x for x in ids_ if x]
            if not got:
                continue
            r = frame(got, boxes)
            if r:
                rects.append(r)
            for col, pid in enumerate(ids_):
                if pid:
                    q[13 + col - 4 * row] = mpc(pid)
        key = page["key"]
        tabs.append(O(("version", 1), ("tabName", page["name"]), ("fnKeyIndex", key),
                      ("fnKeySubIndex", 0), ("componentDefinition", component([], kids)),
                      ("qlinkBoundsData", [box(r) for r in rects])))
        qmaps.append(O(("Tab", key + 1), ("SubTab", 1), ("Bank Direction", "RowTopDown"),
                       ("Q-Links", O(*[("Q-Link %d" % n, q.get(n, -1)) for n in range(1, 17)]))))

    repl = [O(("key", "Parameter %d" % k), ("value", v)) for k, v in sorted(names.items())]
    tui = O(("pageData", O(("version", 1),
                           ("componentDefinitions", O(("version", 2), ("importFiles", []),
                                                      ("localComponentDefinitions", [O(("key", k), ("value", v)) for k, v in defs]))),
                           ("replacementStrings", repl),
                           ("info", O(("version", 1), ("type", "CompleteDescription"))),
                           ("tabs", tabs))))
    ql = O(("version", 4), ("info", O(("version", 1), ("type", "CompleteDescription"))),
           ("Screen Mode Q-Links", O(("version", 4), ("map", qmaps))),
           ("Program Mode Q-Links", O(*[("Q-Link %d" % (n + 1), mpc(pid)) for n, pid in enumerate(PROGRAM_QLINKS)])))
    with open(os.path.join(out, "TUI.json"), "w") as f:
        f.write(dump(tui) + "\n")
    with open(os.path.join(out, "Q-Links.json"), "w") as f:
        f.write(dump(ql) + "\n")
    try:                                 # (256 colours: the pictures a fraction of the size)
        from PIL import Image
    except ImportError:
        Image = None
        print("(no Pillow: the pictures copied as they are, many times larger)")
    for f in sorted(used):
        if Image:
            im = Image.open(os.path.join(src, f)).convert("RGB")
            im.quantize(256, method=Image.MEDIANCUT, dither=Image.NONE).save(os.path.join(out, f), optimize=True)
        else:
            shutil.copy2(os.path.join(src, f), os.path.join(out, f))
    print("TUI.json: %d tabs, %d components; Q-Links.json; %d pictures in %s" % (len(tabs), len(defs), len(used), out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
