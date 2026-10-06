#!/usr/bin/env python3
"""Convert Meta's jollybot.gif into LVGL RGB565 frame arrays for the grinder.

Reads the GIF from $JOLLY_GIF, or <repo>/../muse-gadget-sdk/esp32/avatar/jollybot.gif,
or ~/workspace/coffee-gadgets/muse-gadget-sdk/esp32/avatar/jollybot.gif.

Writes src/muse/jolly_anim.c + jolly_anim.h (both gitignored).

No Meta art is committed to the repo: the Jolly artwork is Meta-copyrighted
(see the gadget SDK's esp32/AGENTS.md — the Apache license does not cover it).
Only this script (which contains no art) is committed; the generated frames
stay on the builder's machine for personal use.

Usage: python3 tools/gen_jolly.py [--gif PATH] [--out-dir src/muse]
"""

import argparse
import os
import pathlib
import sys

FRAME_W = 112
FRAME_H = 112
FRAME_STEP = 4          # every 4th GIF frame (40ms -> 160ms per frame)
BLACK_THRESHOLD = 16    # pixels darker than this count as background
STRAIN_FRAME_MS = 110   # straining loop speed

# Straining pose, in jollybot.gif's 320x320 source pixels. The art is drawn on
# a 5px pixel-art grid; these boxes cover the neutral pose (GIF frame 0).
STRAIN_BASE_FRAME = 0
ART_CELL = 5
EYE_L = (125, 120)            # top-left of the 4x4-cell eye
EYE_R = (175, 120)
MOUTH = (145, 145, 175, 165)  # box holding the smile
FACE = (105, 100, 215, 172)   # skin inside the hood opening
CHEEKS = [(110, 145), (195, 145)]
FEET_Y = 303                  # squash pivots on the feet

REPO = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_CANDIDATES = [
    pathlib.Path(os.environ["JOLLY_GIF"]) if os.environ.get("JOLLY_GIF") else None,
    REPO.parent / "muse-gadget-sdk" / "esp32" / "avatar" / "jollybot.gif",
    pathlib.Path.home() / "workspace" / "coffee-gadgets" / "muse-gadget-sdk"
    / "esp32" / "avatar" / "jollybot.gif",
]


def find_gif(explicit):
    if explicit:
        p = pathlib.Path(explicit)
        if p.is_file():
            return p
        sys.exit(f"gen_jolly: GIF not found at {p}")
    for c in DEFAULT_CANDIDATES:
        if c and c.is_file():
            return c
    sys.exit(
        "gen_jolly: jollybot.gif not found.\n"
        "  Set $JOLLY_GIF to esp32/avatar/jollybot.gif in your local\n"
        "  muse-gadget-sdk clone, or place the clone next to this repo as\n"
        "  ../muse-gadget-sdk."
    )


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main():
    from PIL import Image

    ap = argparse.ArgumentParser()
    ap.add_argument("--gif", default=None)
    ap.add_argument("--out-dir", default=str(REPO / "src" / "muse"))
    ap.add_argument("--preview", default=None, help="also write preview GIFs to this dir")
    args = ap.parse_args()

    gif_path = find_gif(args.gif)
    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"gen_jolly: reading {gif_path}")

    gif = Image.open(gif_path)
    n = gif.n_frames
    idxs = list(range(0, n, FRAME_STEP))
    frames = []
    for i in idxs:
        gif.seek(i)
        frames.append(gif.convert("RGB"))

    # Union bbox of non-black pixels across sampled frames.
    x0, y0, x1, y1 = gif.width, gif.height, 0, 0
    for fr in frames:
        px = fr.load()
        for y in range(0, gif.height, 2):
            for x in range(0, gif.width, 2):
                r, g, b = px[x, y]
                if r > BLACK_THRESHOLD or g > BLACK_THRESHOLD or b > BLACK_THRESHOLD:
                    if x < x0: x0 = x
                    if y < y0: y0 = y
                    if x > x1: x1 = x
                    if y > y1: y1 = y
    pad = 6
    x0, y0 = max(0, x0 - pad), max(0, y0 - pad)
    x1, y1 = min(gif.width, x1 + pad), min(gif.height, y1 + pad)

    crop_box = (x0, y0, x1, y1)
    idle = [to_rgb565(fr, crop_box) for fr in frames]

    gif.seek(STRAIN_BASE_FRAME)
    base = gif.convert("RGB")
    strain = [to_rgb565(f, crop_box) for f in make_strain_frames(base)]

    total = len(idle) + len(strain)
    print(f"gen_jolly: {len(idle)} idle + {len(strain)} straining frames, "
          f"{FRAME_W}x{FRAME_H} RGB565 ({len(idle[0]) * total // 1024} KiB flash)")

    write_sources(out_dir, idle, strain)
    print(f"gen_jolly: wrote {out_dir / 'jolly_anim.c'} and jolly_anim.h")

    if args.preview:
        write_previews(pathlib.Path(args.preview), idle, strain)


def to_rgb565(frame, crop_box):
    from PIL import Image

    small = frame.crop(crop_box).resize((FRAME_W, FRAME_H), Image.LANCZOS)
    px = small.load()
    buf = bytearray()
    for y in range(FRAME_H):
        for x in range(FRAME_W):
            v = rgb565(*px[x, y])
            buf.append(v & 0xFF)
            buf.append(v >> 8)
    return bytes(buf)


def from_rgb565(buf):
    from PIL import Image

    img = Image.new("RGB", (FRAME_W, FRAME_H))
    px = img.load()
    for i in range(FRAME_W * FRAME_H):
        v = buf[2 * i] | (buf[2 * i + 1] << 8)
        r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
        px[i % FRAME_W, i // FRAME_W] = (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)
    return img


def cells(draw, x0, y0, pattern, color):
    """Paint a pixel-art pattern ('#' = filled cell) on the 5px art grid."""
    for r, row in enumerate(pattern):
        for c, ch in enumerate(row):
            if ch == "#":
                x, y = x0 + c * ART_CELL, y0 + r * ART_CELL
                draw.rectangle([x, y, x + ART_CELL - 1, y + ART_CELL - 1], fill=color)


def make_strain_frames(base):
    """Jolly squeezing hard: eyes shut, teeth gritted, face flushed, crouching
    and shaking, sweating, with a strain vein. No poop is drawn — the real
    grounds falling from the chute are the punchline."""
    from PIL import Image, ImageDraw

    ink = (34, 22, 24)
    sweat = (120, 190, 255)
    sweat_hi = (225, 245, 255)
    vein = (230, 40, 50)

    # Keep only Jolly: the largest connected non-background region. Drops the
    # sparkles and the dotted background.
    face = base.copy()
    px = face.load()
    w, h = face.size

    def is_bg(p):
        r, g, b = p
        return max(r, g, b) <= BLACK_THRESHOLD * 3 or b > r + 15

    seen = bytearray(w * h)
    best = []
    for sy in range(h):
        for sx in range(w):
            if seen[sy * w + sx] or is_bg(px[sx, sy]):
                continue
            region, stack = [], [(sx, sy)]
            seen[sy * w + sx] = 1
            while stack:
                x, y = stack.pop()
                region.append((x, y))
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h and not seen[ny * w + nx] and not is_bg(px[nx, ny]):
                        seen[ny * w + nx] = 1
                        stack.append((nx, ny))
            if len(region) > len(best):
                best = region
    keep = bytearray(w * h)
    for x, y in best:
        keep[y * w + x] = 1
    for y in range(h):
        for x in range(w):
            if not keep[y * w + x]:
                px[x, y] = (0, 0, 0)

    skin = px[160, 132]
    d = ImageDraw.Draw(face)
    # Erase the open eyes and smile.
    for ex, ey in (EYE_L, EYE_R):
        d.rectangle([ex - 5, ey - 5, ex + 4 * ART_CELL + 4, ey + 4 * ART_CELL + 4], fill=skin)
    d.rectangle(MOUTH, fill=skin)
    # Squeezed-shut eyes: > <
    cells(d, EYE_L[0], EYE_L[1] - 2, ["#...", ".##.", "...#", ".##.", "#..."], ink)
    cells(d, EYE_R[0], EYE_R[1] - 2, ["...#", ".##.", "#...", ".##.", "...#"], ink)
    # Clenched zigzag mouth.
    cells(d, MOUTH[0] - 5, MOUTH[1] + 5, [".#.#.#.", "#.#.#.#"], ink)

    frames = []
    # (squash, shake_x, flush, sweat_drop, vein_big)
    beats = [(1.00, -5, 0.30, 0, False), (0.94, 5, 0.42, 5, True),
             (0.90, -5, 0.52, 10, True), (0.94, 5, 0.42, 15, False)]
    for squash, shake, flush, drop, vein_big in beats:
        f = face.copy()
        fpx = f.load()
        # Flush the face red.
        x0, y0, x1, y1 = FACE
        for y in range(y0, y1):
            for x in range(x0, x1):
                r, g, b = fpx[x, y]
                if r + g + b > 450:  # skin, not ink
                    fpx[x, y] = (min(255, int(r + (255 - r) * flush)),
                                 int(g * (1 - flush * 0.55)), int(b * (1 - flush * 0.55)))
        fd = ImageDraw.Draw(f)
        # Big effort blush.
        for cx, cy in CHEEKS:
            cells(fd, cx, cy, ["###", "###"], (240, 90, 110))
        # Sweat drops sliding down.
        for sx, sy in ((80, 95), (225, 105)):
            cells(fd, sx, sy + drop, [".#.", "###", "###", ".#."], sweat)
            cells(fd, sx + 5, sy + drop + 5, ["#"], sweat_hi)
        # Strain vein (pulses).
        vx, vy = 205, 50
        if vein_big:
            cells(fd, vx, vy, ["##.##", "#...#", ".....", "#...#", "##.##"], vein)
        else:
            cells(fd, vx + 5, vy + 5, ["#.#", "...", "#.#"], vein)

        # Crouch: squash on the feet, widen a little; then shake sideways.
        w, h = f.size
        nh = int(h * squash)
        nw = int(w * (1 + (1 - squash) * 0.6))
        squashed = f.resize((nw, nh), Image.NEAREST)
        out = Image.new("RGB", (w, h))
        out.paste(squashed, ((w - nw) // 2 + shake, int(FEET_Y - FEET_Y * squash)))
        frames.append(out)
    return frames


def write_sources(out_dir, idle, strain):
    h = f"""// Generated by tools/gen_jolly.py from jollybot.gif — DO NOT EDIT, DO NOT COMMIT.
// Meta-copyrighted artwork; generated at build time for personal use only.
#pragma once
#include <lvgl.h>
#include <stdint.h>

#define JOLLY_FRAME_W {FRAME_W}
#define JOLLY_FRAME_H {FRAME_H}
#define JOLLY_FRAME_COUNT {len(idle)}
#define JOLLY_FRAME_MS {40 * FRAME_STEP}
#define JOLLY_STRAIN_COUNT {len(strain)}
#define JOLLY_STRAIN_FRAME_MS {STRAIN_FRAME_MS}

#ifdef __cplusplus
extern "C" {{
#endif

const lv_image_dsc_t* jolly_get_frame(int i);
const lv_image_dsc_t* jolly_get_strain_frame(int i);

#ifdef __cplusplus
}}
#endif
"""
    (out_dir / "jolly_anim.h").write_text(h)

    parts = [
        "// Generated by tools/gen_jolly.py from jollybot.gif — DO NOT EDIT, DO NOT COMMIT.",
        "// Meta-copyrighted artwork; generated at build time for personal use only.",
        '#include "jolly_anim.h"',
        "",
    ]
    for name, bufs, count_macro in (("jolly", idle, "JOLLY_FRAME_COUNT"),
                                    ("jolly_strain", strain, "JOLLY_STRAIN_COUNT")):
        for i, buf in enumerate(bufs):
            hexbytes = ", ".join(f"0x{b:02x}" for b in buf)
            parts.append(f"static const uint8_t {name}_frame_{i}[{len(buf)}] = {{{hexbytes}}};")
        parts.append("")
        parts.append(f"static const lv_image_dsc_t {name}_frames[{count_macro}] = {{")
        for i in range(len(bufs)):
            parts.append(
                f"    {{ .header = {{ .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,"
                f" .flags = 0, .w = JOLLY_FRAME_W, .h = JOLLY_FRAME_H, .stride = JOLLY_FRAME_W * 2 }},"
                f" .data_size = sizeof({name}_frame_{i}), .data = {name}_frame_{i} }},"
            )
        parts.append("};")
        parts.append("")
    for fn, name, count_macro in (("jolly_get_frame", "jolly", "JOLLY_FRAME_COUNT"),
                                  ("jolly_get_strain_frame", "jolly_strain", "JOLLY_STRAIN_COUNT")):
        parts.append(f"const lv_image_dsc_t* {fn}(int i) {{")
        parts.append("    if (i < 0) i = 0;")
        parts.append(f"    return &{name}_frames[i % {count_macro}];")
        parts.append("}")
        parts.append("")
    (out_dir / "jolly_anim.c").write_text("\n".join(parts))


def write_previews(preview_dir, idle, strain):
    """Animated GIFs of exactly what the device shows (decoded back from
    RGB565), at 3x for viewing. Not for committing — same art restrictions."""
    from PIL import Image

    preview_dir.mkdir(parents=True, exist_ok=True)
    for name, bufs, ms in (("jolly_idle.gif", idle, 40 * FRAME_STEP),
                           ("jolly_straining.gif", strain, STRAIN_FRAME_MS)):
        imgs = [from_rgb565(b).resize((FRAME_W * 3, FRAME_H * 3), Image.NEAREST) for b in bufs]
        imgs[0].save(preview_dir / name, save_all=True, append_images=imgs[1:],
                     duration=ms, loop=0)
        print(f"gen_jolly: preview {preview_dir / name}")


if __name__ == "__main__":
    main()
