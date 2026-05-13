"""Render the Node 2.1 BIM placement UX wireframe.

3 phone screens side-by-side at iPhone 16 Pro proportions, labeled with
the placement state machine. Saved to ./node-2.1-placement-ux.png.
"""

from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
import math

OUT = Path(__file__).parent / "node-2.1-placement-ux.png"

PHONE_W, PHONE_H = 1290, 2796
GAP = 120
MARGIN = 140
TITLE_H = 280
SUBTITLE_H = 80

CANVAS_W = MARGIN * 2 + PHONE_W * 3 + GAP * 2
CANVAS_H = TITLE_H + SUBTITLE_H + PHONE_H + MARGIN

BG = (24, 26, 30)
PHONE_BEZEL = (12, 12, 14)
PHONE_SCREEN = (58, 64, 70)
LIDAR_CYAN = (0, 255, 204)
BIM_ORANGE = (255, 133, 51)
TAP_YELLOW = (255, 214, 10)
TEXT_WHITE = (245, 248, 252)
TEXT_DIM = (165, 175, 188)
PANEL_DARK = (0, 0, 0, 153)
DROP_SHADOW = (0, 0, 0, 220)


def load_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    candidates = [
        "C:/Windows/Fonts/" + ("seguisb.ttf" if bold else "segoeui.ttf"),
        "C:/Windows/Fonts/" + ("arialbd.ttf" if bold else "arial.ttf"),
    ]
    for path in candidates:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


def rounded_rect(draw: ImageDraw.ImageDraw, box, radius, fill=None, outline=None, width=1):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def draw_phone_frame(canvas: Image.Image, x: int, y: int) -> tuple[int, int, int, int]:
    """Draws the phone bezel + screen, returns (sx, sy, sw, sh) of the inner screen rect."""
    bezel_pad = 36
    radius = 180
    draw = ImageDraw.Draw(canvas)
    rounded_rect(draw, (x, y, x + PHONE_W, y + PHONE_H), radius, fill=PHONE_BEZEL)
    sx, sy = x + bezel_pad, y + bezel_pad
    sw, sh = PHONE_W - bezel_pad * 2, PHONE_H - bezel_pad * 2
    rounded_rect(draw, (sx, sy, sx + sw, sy + sh), radius - bezel_pad, fill=PHONE_SCREEN)

    # Dynamic Island
    di_w, di_h = 360, 90
    di_x = sx + (sw - di_w) // 2
    di_y = sy + 40
    rounded_rect(draw, (di_x, di_y, di_x + di_w, di_y + di_h), 45, fill=(8, 8, 10))

    return sx, sy, sw, sh


def draw_lidar_mesh(canvas: Image.Image, sx: int, sy: int, sw: int, sh: int):
    """Faint cyan triangulated wireframe across the lower 60% of the screen
    suggesting a LiDAR-scanned floor."""
    overlay = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    horizon = int(sh * 0.42)
    rows = 16
    cols = 14
    cell_w = sw / cols
    cell_h = (sh - horizon) / rows
    pts = {}
    for r in range(rows + 1):
        # perspective foreshortening — rows further away are tighter
        scale = 0.35 + 0.85 * (r / rows)
        y = horizon + (sh - horizon) * (1 - (1 - r / rows) ** 1.6)
        for c in range(cols + 1):
            cx = sw / 2 + (c - cols / 2) * cell_w * scale
            pts[(r, c)] = (cx, y)
    line_color = (*LIDAR_CYAN, 110)
    for r in range(rows + 1):
        for c in range(cols):
            od.line([pts[(r, c)], pts[(r, c + 1)]], fill=line_color, width=3)
    for c in range(cols + 1):
        for r in range(rows):
            od.line([pts[(r, c)], pts[(r + 1, c)]], fill=line_color, width=3)
    canvas.paste(overlay, (sx, sy), overlay)


def draw_panel(canvas: Image.Image, sx, sy, sw, sh, height_frac, padding):
    """Bottom-anchored translucent dark panel. Returns its inner content box."""
    overlay = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    panel_h = int(sh * height_frac)
    panel_top = sh - panel_h - padding
    panel_box = (padding, panel_top, sw - padding, sh - padding)
    od.rounded_rectangle(panel_box, radius=60, fill=PANEL_DARK)
    canvas.paste(overlay, (sx, sy), overlay)
    return (sx + panel_box[0], sy + panel_box[1], sx + panel_box[2], sy + panel_box[3])


def text_with_shadow(draw, xy, text, font, fill=TEXT_WHITE, anchor="mm"):
    x, y = xy
    draw.text((x + 3, y + 3), text, font=font, fill=(0, 0, 0, 220), anchor=anchor)
    draw.text((x, y), text, font=font, fill=fill, anchor=anchor)


def reticle(draw, cx, cy, radius=80, color=TEXT_WHITE, dashed=False):
    if dashed:
        # dashed circle approximation
        steps = 36
        for i in range(0, steps, 2):
            a0 = 2 * math.pi * i / steps
            a1 = 2 * math.pi * (i + 1) / steps
            x0, y0 = cx + radius * math.cos(a0), cy + radius * math.sin(a0)
            x1, y1 = cx + radius * math.cos(a1), cy + radius * math.sin(a1)
            draw.line([(x0, y0), (x1, y1)], fill=color, width=5)
    else:
        draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), outline=color, width=5)
    # crosshair tick marks
    tick = 18
    draw.line([(cx - radius - tick, cy), (cx - radius - tick - 22, cy)], fill=color, width=4)
    draw.line([(cx + radius + tick, cy), (cx + radius + tick + 22, cy)], fill=color, width=4)
    draw.line([(cx, cy - radius - tick), (cx, cy - radius - tick - 22)], fill=color, width=4)
    draw.line([(cx, cy + radius + tick), (cx, cy + radius + tick + 22)], fill=color, width=4)


def filled_marker(draw, cx, cy, color=TAP_YELLOW, r=34):
    # outer halo
    halo = Image.new("RGBA", (r * 6, r * 6), (0, 0, 0, 0))
    hd = ImageDraw.Draw(halo)
    hd.ellipse((r, r, r * 5, r * 5), fill=(*color, 70))
    return halo  # caller pastes


def dashed_line(draw, p0, p1, color, width=5, dash=24, gap=18):
    x0, y0 = p0
    x1, y1 = p1
    dx, dy = x1 - x0, y1 - y0
    dist = math.hypot(dx, dy)
    if dist == 0:
        return
    ux, uy = dx / dist, dy / dist
    pos = 0.0
    while pos < dist:
        sx, sy = x0 + ux * pos, y0 + uy * pos
        end = min(pos + dash, dist)
        ex, ey = x0 + ux * end, y0 + uy * end
        draw.line([(sx, sy), (ex, ey)], fill=color, width=width)
        pos += dash + gap


def draw_bim_wireframe(canvas, sx, sy, sw, sh, anchor_xy):
    """Translucent orange isometric wireframe building dropped at anchor_xy
    (canvas coords). A simple 3-storey rectilinear massing — reads as
    'building' immediately."""
    overlay = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    ax, ay = anchor_xy
    # Footprint dimensions in screen space
    fp_w, fp_d = 520, 240
    storey = 200
    storeys = 3
    # Isometric vector basis (slight angle)
    vx = (1.0, 0.45)  # along building width
    vy = (-0.65, 0.32)  # along building depth
    vz = (0, -1.0)  # up

    def proj(local):
        lx, ly, lz = local
        return (
            ax + lx * vx[0] + ly * vy[0] + lz * vz[0],
            ay + lx * vx[1] + ly * vy[1] + lz * vz[1],
        )

    fill = (*BIM_ORANGE, 65)
    edge = (*BIM_ORANGE, 255)

    # Storey-by-storey: corners + verticals + roof
    base_corners = [
        (-fp_w / 2, -fp_d / 2, 0),
        (fp_w / 2, -fp_d / 2, 0),
        (fp_w / 2, fp_d / 2, 0),
        (-fp_w / 2, fp_d / 2, 0),
    ]
    total_h = storey * storeys
    top_corners = [(c[0], c[1], total_h) for c in base_corners]

    # Filled side panels (very translucent) — just front-right + right-side for depth read
    front = [proj(base_corners[0]), proj(base_corners[1]),
             proj(top_corners[1]), proj(top_corners[0])]
    side = [proj(base_corners[1]), proj(base_corners[2]),
            proj(top_corners[2]), proj(top_corners[1])]
    od.polygon(front, fill=fill)
    od.polygon(side, fill=fill)

    # Vertical edges
    for b, t in zip(base_corners, top_corners):
        od.line([proj(b), proj(t)], fill=edge, width=8)
    # Base + roof rectangles
    for ring in (base_corners, top_corners):
        for i in range(4):
            od.line([proj(ring[i]), proj(ring[(i + 1) % 4])], fill=edge, width=8)
    # Floor lines (slab divisions)
    for s in range(1, storeys):
        z = storey * s
        floor = [(c[0], c[1], z) for c in base_corners]
        for i in range(4):
            od.line([proj(floor[i]), proj(floor[(i + 1) % 4])],
                    fill=(*BIM_ORANGE, 180), width=5)

    canvas.paste(overlay, (0, 0), overlay)


def draw_state_1(canvas, sx, sy, sw, sh, font_md, font_sm):
    draw_lidar_mesh(canvas, sx, sy, sw, sh)
    panel = draw_panel(canvas, sx, sy, sw, sh, height_frac=0.11, padding=60)
    px = (panel[0] + panel[2]) // 2
    py = (panel[1] + panel[3]) // 2
    d = ImageDraw.Draw(canvas)
    text_with_shadow(d, (px, py - 30), "TAP TO SET BIM ORIGIN", font_md, anchor="mm")
    text_with_shadow(d, (px, py + 50), "Aim the reticle at the floor", font_sm, fill=TEXT_DIM, anchor="mm")
    # Center reticle
    cx, cy = sx + sw // 2, sy + int(sh * 0.48)
    reticle(d, cx, cy, radius=110, color=TEXT_WHITE)


def draw_state_2(canvas, sx, sy, sw, sh, font_md, font_sm):
    draw_lidar_mesh(canvas, sx, sy, sw, sh)
    d = ImageDraw.Draw(canvas)
    # First-tap marker — lower-left of the floor area
    m_cx = sx + int(sw * 0.32)
    m_cy = sy + int(sh * 0.62)
    # halo
    halo = Image.new("RGBA", (260, 260), (0, 0, 0, 0))
    hd = ImageDraw.Draw(halo)
    hd.ellipse((40, 40, 220, 220), fill=(*TAP_YELLOW, 60))
    canvas.paste(halo, (m_cx - 130, m_cy - 130), halo)
    d.ellipse((m_cx - 38, m_cy - 38, m_cx + 38, m_cy + 38), fill=TAP_YELLOW, outline=(255, 255, 255), width=4)
    # Reticle at center
    cx, cy = sx + sw // 2, sy + int(sh * 0.48)
    reticle(d, cx, cy, radius=110, color=TEXT_WHITE)
    # Dashed line marker → reticle
    dashed_line(d, (m_cx, m_cy), (cx, cy), TAP_YELLOW, width=6, dash=28, gap=20)
    # Panel
    panel = draw_panel(canvas, sx, sy, sw, sh, height_frac=0.11, padding=60)
    px = (panel[0] + panel[2]) // 2
    py = (panel[1] + panel[3]) // 2
    text_with_shadow(d, (px, py - 30), "TAP TO SET FORWARD DIRECTION", font_md, anchor="mm")
    text_with_shadow(d, (px, py + 50), "Defines which way the building faces", font_sm, fill=TEXT_DIM, anchor="mm")


def draw_state_3(canvas, sx, sy, sw, sh, font_md, font_sm, font_xs):
    draw_lidar_mesh(canvas, sx, sy, sw, sh)
    # Anchor where the BIM sits on the floor
    anchor = (sx + int(sw * 0.5), sy + int(sh * 0.62))
    draw_bim_wireframe(canvas, sx, sy, sw, sh, anchor)
    d = ImageDraw.Draw(canvas)

    # Slim toolbar — three icon buttons
    tb_h = int(sh * 0.075)
    tb_pad = 60
    tb_box = (sx + tb_pad, sy + sh - tb_h - tb_pad, sx + sw - tb_pad, sy + sh - tb_pad)
    overlay = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rounded_rectangle(
        (tb_box[0] - sx, tb_box[1] - sy, tb_box[2] - sx, tb_box[3] - sy),
        radius=tb_h // 2, fill=PANEL_DARK
    )
    canvas.paste(overlay, (sx, sy), overlay)

    # Three slots
    tb_w = tb_box[2] - tb_box[0]
    tb_cy = (tb_box[1] + tb_box[3]) // 2
    slot_w = tb_w // 3
    centers = [tb_box[0] + slot_w * i + slot_w // 2 for i in range(3)]

    # Slot 1: Reset (X in circle)
    cx = centers[0]
    r = tb_h // 3
    d.ellipse((cx - r, tb_cy - r, cx + r, tb_cy + r), outline=TEXT_WHITE, width=5)
    off = int(r * 0.55)
    d.line([(cx - off, tb_cy - off), (cx + off, tb_cy + off)], fill=TEXT_WHITE, width=5)
    d.line([(cx + off, tb_cy - off), (cx - off, tb_cy + off)], fill=TEXT_WHITE, width=5)
    text_with_shadow(d, (cx, tb_box[3] + 56), "RESET", font_xs, fill=TEXT_DIM, anchor="mm")

    # Slot 2: Opacity slider
    cx = centers[1]
    track_w = int(slot_w * 0.62)
    track_h = 10
    d.rounded_rectangle(
        (cx - track_w // 2, tb_cy - track_h // 2, cx + track_w // 2, tb_cy + track_h // 2),
        radius=track_h // 2, fill=(255, 255, 255, 90), outline=TEXT_DIM, width=2
    )
    # Filled portion
    filled = int(track_w * 0.65)
    d.rounded_rectangle(
        (cx - track_w // 2, tb_cy - track_h // 2, cx - track_w // 2 + filled, tb_cy + track_h // 2),
        radius=track_h // 2, fill=BIM_ORANGE
    )
    # Knob
    knob_x = cx - track_w // 2 + filled
    knob_r = 22
    d.ellipse((knob_x - knob_r, tb_cy - knob_r, knob_x + knob_r, tb_cy + knob_r),
              fill=TEXT_WHITE, outline=BIM_ORANGE, width=4)
    text_with_shadow(d, (cx, tb_box[3] + 56), "OPACITY", font_xs, fill=TEXT_DIM, anchor="mm")

    # Slot 3: Lock icon
    cx = centers[2]
    body_w, body_h = 56, 50
    body_top = tb_cy - 6
    d.rounded_rectangle((cx - body_w // 2, body_top, cx + body_w // 2, body_top + body_h),
                        radius=8, outline=TEXT_WHITE, width=5)
    # shackle
    d.arc((cx - body_w // 2 + 8, body_top - body_h + 4, cx + body_w // 2 - 8, body_top + 8),
          start=180, end=360, fill=TEXT_WHITE, width=5)
    # keyhole
    d.ellipse((cx - 5, tb_cy + 6, cx + 5, tb_cy + 16), fill=TEXT_WHITE)
    text_with_shadow(d, (cx, tb_box[3] + 56), "LOCK", font_xs, fill=TEXT_DIM, anchor="mm")


def main():
    canvas = Image.new("RGB", (CANVAS_W, CANVAS_H), BG)
    d = ImageDraw.Draw(canvas)

    title_font = load_font(120, bold=True)
    sub_font = load_font(60)
    label_font = load_font(76, bold=True)
    body_font = load_font(54)
    panel_md = load_font(60, bold=True)
    panel_sm = load_font(42)
    icon_label = load_font(34, bold=True)

    # Top title
    d.text((MARGIN, 90), "SiteSync AR — Node 2.1", font=title_font, fill=TEXT_WHITE)
    d.text((MARGIN, 220), "BIM Placement HUD · 3-state wireframe",
           font=sub_font, fill=TEXT_DIM)
    # Top-right meta
    meta = "iPhone 16 Pro · 1290×2796 portrait · v0"
    d.text((CANVAS_W - MARGIN, 230), meta, font=sub_font, fill=TEXT_DIM, anchor="rs")

    # Place 3 phones
    px = MARGIN
    py = TITLE_H + SUBTITLE_H
    phones = []
    labels = ["STATE 1 · READY", "STATE 2 · FORWARD", "STATE 3 · PLACED"]
    for i in range(3):
        sx, sy, sw, sh = draw_phone_frame(canvas, px, py)
        phones.append((sx, sy, sw, sh))
        # Label below phone
        cx_phone = px + PHONE_W // 2
        d.text((cx_phone, py + PHONE_H + 80), labels[i], font=label_font, fill=TEXT_WHITE, anchor="ms")
        px += PHONE_W + GAP

    # Render the three states
    draw_state_1(canvas, *phones[0], panel_md, panel_sm)
    draw_state_2(canvas, *phones[1], panel_md, panel_sm)
    draw_state_3(canvas, *phones[2], panel_md, panel_sm, icon_label)

    canvas.save(OUT, "PNG", optimize=True)
    print(f"saved {OUT} ({OUT.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
