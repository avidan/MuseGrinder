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

# Both loops show Jolly on the toilet with the same tight crop (source px),
# so Jolly stays put when switching between dancing and straining. The
# bottom edge sits just under the pedestal.
FRAME_W = 112
FRAME_H = 124
SCENE_CROP = (28, 16, 292, 310)
HAPPY_SQUASH = 0.90           # sitting down on the seat
FRAME_STEP = 5          # every 5th GIF frame (40ms -> 200ms per frame)
BLACK_THRESHOLD = 16    # pixels darker than this count as background
STRAIN_FRAME_MS = 160   # straining loop speed

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

    idle = [to_rgb565(f, SCENE_CROP) for f in make_happy_frames(frames)]

    gif.seek(STRAIN_BASE_FRAME)
    base = gif.convert("RGB")
    strain = [to_rgb565(f, SCENE_CROP) for f in make_strain_frames(base)]

    kib = (sum(map(len, idle)) + sum(map(len, strain))) // 1024
    print(f"gen_jolly: {len(idle)} dancing + {len(strain)} straining frames, "
          f"{FRAME_W}x{FRAME_H} RGB565 ({kib} KiB flash)")

    write_sources(out_dir, idle, strain)
    print(f"gen_jolly: wrote {out_dir / 'jolly_anim.c'} and jolly_anim.h")

    if args.preview:
        write_previews(pathlib.Path(args.preview), idle, strain)


def to_rgb565(frame, crop_box, w=FRAME_W, h=FRAME_H):
    from PIL import Image

    small = frame.crop(crop_box).resize((w, h), Image.LANCZOS)
    px = small.load()
    buf = bytearray()
    for y in range(h):
        for x in range(w):
            v = rgb565(*px[x, y])
            buf.append(v & 0xFF)
            buf.append(v >> 8)
    return bytes(buf)


def from_rgb565(buf, w=FRAME_W, h=FRAME_H):
    from PIL import Image

    img = Image.new("RGB", (w, h))
    px = img.load()
    for i in range(w * h):
        v = buf[2 * i] | (buf[2 * i + 1] << 8)
        r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
        px[i % w, i // w] = (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)
    return img


def cells(draw, x0, y0, pattern, color):
    """Paint a pixel-art pattern ('#' = filled cell) on the 5px art grid."""
    for r, row in enumerate(pattern):
        for c, ch in enumerate(row):
            if ch == "#":
                x, y = x0 + c * ART_CELL, y0 + r * ART_CELL
                draw.rectangle([x, y, x + ART_CELL - 1, y + ART_CELL - 1], fill=color)


def extract_jolly(frame, keep_hearts):
    """Cut Jolly out by silhouette. Returns (image on black, mask).

    Background is only what's reachable from the image edge through dark or
    lavender (sparkle/dot) pixels, so dark eyes and outlines inside Jolly stay
    opaque. Keeps the largest shape (the body), plus the pink hearts if asked.
    """
    from PIL import Image

    src = frame.load()
    w, h = frame.size

    def is_bg(p):
        r, g, b = p
        return max(r, g, b) <= BLACK_THRESHOLD * 3 or b > r + 15

    # Flood the background in from the edges.
    outside = bytearray(w * h)
    stack = [(x, y) for x in range(w) for y in (0, h - 1)] + [(x, y) for y in range(h) for x in (0, w - 1)]
    while stack:
        x, y = stack.pop()
        i = y * w + x
        if outside[i] or not is_bg(src[x, y]):
            continue
        outside[i] = 1
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < w and 0 <= ny < h and not outside[ny * w + nx]:
                stack.append((nx, ny))

    # Group what's left into shapes.
    label = [0] * (w * h)
    shapes = []
    for sy in range(h):
        for sx in range(w):
            i0 = sy * w + sx
            if outside[i0] or label[i0]:
                continue
            shape, stack = [], [(sx, sy)]
            label[i0] = len(shapes) + 1
            while stack:
                x, y = stack.pop()
                shape.append((x, y))
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    j = ny * w + nx
                    if 0 <= nx < w and 0 <= ny < h and not outside[j] and not label[j]:
                        label[j] = label[i0]
                        stack.append((nx, ny))
            shapes.append(shape)

    def is_heart(shape):
        r = sum(src[x, y][0] for x, y in shape) / len(shape)
        g = sum(src[x, y][1] for x, y in shape) / len(shape)
        return r - g > 60

    largest = max(shapes, key=len)
    out = Image.new("RGB", (w, h))
    mask = Image.new("L", (w, h))
    opx, mpx = out.load(), mask.load()
    for shape in shapes:
        if shape is largest or (keep_hearts and is_heart(shape)):
            for x, y in shape:
                opx[x, y] = src[x, y]
                mpx[x, y] = 255
    return out, mask


def seat_on_toilet(jolly, mask, squash, shake=0):
    """Compose Jolly onto the toilet: tank behind, seat and bowl in front.
    squash < 1 lowers Jolly onto the seat (pivoting on the feet)."""
    from PIL import Image, ImageChops, ImageDraw

    # Anything drawn onto Jolly outside the silhouette (sweat, veins) counts too.
    mask = ImageChops.lighter(mask, jolly.convert("L").point(lambda v: 255 if v > 0 else 0))
    w, h = jolly.size
    nh = int(h * squash)
    nw = int(w * (1 + (1 - squash) * 0.8))
    squashed = jolly.resize((nw, nh), Image.NEAREST)
    squashed_mask = mask.resize((nw, nh), Image.NEAREST)
    out = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(out)
    draw_toilet_tank(d)
    out.paste(squashed, ((w - nw) // 2 + shake, int(FEET_Y - FEET_Y * squash)), squashed_mask)
    draw_toilet_bowl(d)
    return out


def make_happy_frames(frames):
    """The original dancing loop (arms, bounce, hearts), sitting on the toilet."""
    return [seat_on_toilet(*extract_jolly(f, keep_hearts=True), HAPPY_SQUASH) for f in frames]


def make_strain_frames(base):
    """Jolly squeezing hard: eyes shut, teeth gritted, face flushed, crouching
    and shaking, sweating, with a strain vein. No poop is drawn — the real
    grounds falling from the chute are the punchline."""
    from PIL import Image, ImageDraw

    ink = (34, 22, 24)
    sweat = (120, 190, 255)
    sweat_hi = (225, 245, 255)
    vein = (230, 40, 50)

    face, face_mask = extract_jolly(base, keep_hearts=False)
    px = face.load()

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
    beats = [
        (0.92, -10, 0.45, 0, False),
        (0.82, 10, 0.65, 8, True),
        (0.76, -10, 0.80, 16, True),
        (0.84, 10, 0.60, 24, False),
    ]
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
                                 int(g * (1 - flush * 0.6)), int(b * (1 - flush * 0.6)))
        fd = ImageDraw.Draw(f)
        # Big effort blush.
        for cx, cy in CHEEKS:
            cells(fd, cx - 5, cy, ["####", "####"], (235, 60, 90))
        # Sweat flying off.
        for sx, sy in ((75, 85), (230, 95), (90, 150), (215, 160)):
            cells(fd, sx, sy + drop, [".#.", "###", "###", ".#."], sweat)
            cells(fd, sx + 5, sy + drop + 5, ["#"], sweat_hi)
        # Strain veins (pulse).
        for vx, vy in ((205, 50), (85, 60)):
            if vein_big:
                cells(fd, vx, vy, ["##.##", "#...#", ".....", "#...#", "##.##"], vein)
            else:
                cells(fd, vx + 5, vy + 5, ["#.#", "...", "#.#"], vein)

        # Crouch hard onto the seat, widen, and shake.
        out = seat_on_toilet(f, face_mask, squash, shake)
        od = ImageDraw.Draw(out)
        # Shaking motion lines.
        for ly in (130, 165, 200):
            off = 0 if shake < 0 else 6
            od.rectangle([12 + off, ly, 32 + off, ly + 4], fill=(225, 225, 235))
            od.rectangle([288 - off, ly, 308 - off, ly + 4], fill=(225, 225, 235))
        frames.append(out)
    return frames


TOILET_WHITE = (242, 245, 250)
TOILET_SHADE = (200, 208, 220)
TOILET_LINE = (85, 95, 110)


def draw_toilet_tank(d):
    """Cistern behind Jolly: its lid and flush handle peek out above the head."""
    d.rounded_rectangle([92, 30, 228, 175], radius=10, fill=TOILET_WHITE, outline=TOILET_LINE, width=3)
    d.rectangle([186, 36, 222, 170], fill=TOILET_SHADE)
    d.rounded_rectangle([84, 22, 236, 40], radius=6, fill=TOILET_WHITE, outline=TOILET_LINE, width=3)
    d.rounded_rectangle([100, 52, 128, 60], radius=3, fill=(170, 178, 190), outline=TOILET_LINE, width=2)


def draw_toilet_bowl(d):
    """Seat and bowl in front of Jolly, narrowing to a pedestal."""
    # Bowl body tapering down to the pedestal.
    d.polygon([(72, 262), (248, 262), (214, 292), (106, 292)], fill=TOILET_WHITE, outline=TOILET_LINE)
    d.polygon([(160, 266), (240, 266), (212, 289), (160, 289)], fill=TOILET_SHADE)
    d.polygon([(118, 290), (202, 290), (210, 306), (110, 306)], fill=TOILET_WHITE, outline=TOILET_LINE)
    d.polygon([(160, 292), (200, 292), (206, 304), (160, 304)], fill=TOILET_SHADE)
    # Seat ring Jolly is sitting on (front lip).
    d.rounded_rectangle([58, 244, 262, 266], radius=11, fill=TOILET_WHITE, outline=TOILET_LINE, width=3)
    d.line([(70, 262), (250, 262)], fill=TOILET_SHADE, width=3)


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
#define JOLLY_STRAIN_W {FRAME_W}
#define JOLLY_STRAIN_H {FRAME_H}
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
    for name, bufs, count_macro, wm, hm in (
            ("jolly", idle, "JOLLY_FRAME_COUNT", "JOLLY_FRAME_W", "JOLLY_FRAME_H"),
            ("jolly_strain", strain, "JOLLY_STRAIN_COUNT", "JOLLY_STRAIN_W", "JOLLY_STRAIN_H")):
        for i, buf in enumerate(bufs):
            hexbytes = ", ".join(f"0x{b:02x}" for b in buf)
            parts.append(f"static const uint8_t {name}_frame_{i}[{len(buf)}] = {{{hexbytes}}};")
        parts.append("")
        parts.append(f"static const lv_image_dsc_t {name}_frames[{count_macro}] = {{")
        for i in range(len(bufs)):
            parts.append(
                f"    {{ .header = {{ .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,"
                f" .flags = 0, .w = {wm}, .h = {hm}, .stride = {wm} * 2 }},"
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
    for name, bufs, ms, w, h in (("jolly_idle.gif", idle, 40 * FRAME_STEP, FRAME_W, FRAME_H),
                                 ("jolly_straining.gif", strain, STRAIN_FRAME_MS, FRAME_W, FRAME_H)):
        imgs = [from_rgb565(b, w, h).resize((w * 3, h * 3), Image.NEAREST) for b in bufs]
        imgs[0].save(preview_dir / name, save_all=True, append_images=imgs[1:],
                     duration=ms, loop=0)
        print(f"gen_jolly: preview {preview_dir / name}")


if __name__ == "__main__":
    main()
