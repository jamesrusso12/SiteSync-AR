"""
Applies the SiteSync website design system to existing UMG widgets.
Covers what Python can reach: TextBlock font sizes, colors, button text.
Layout changes (rounded panels, background images, new containers) still
need the UMG designer — see the UI spec in the session notes.

Run with editor OPEN and target widgets NOT open in the designer:
    python3 dev/mcp_send.py exec_file dev/apply_widget_styles.py

Website design token equivalents (linear sRGB, not gamma):
  --cyan    #4080FF  →  (0.063, 0.251, 1.000)   primary labels / accent
  --orange  #FF6B35  →  (1.000, 0.176, 0.024)   CTA buttons
  --white   #F0F5FF  →  (0.878, 0.918, 1.000)   body text
  --muted   #6E89AD  →  (0.168, 0.263, 0.420)   secondary text
  --bg      #06090F  →  (0.005, 0.009, 0.033)   dark background
"""
import unreal

CYAN   = unreal.LinearColor(0.063, 0.251, 1.000, 1.0)
ORANGE = unreal.LinearColor(1.000, 0.176, 0.024, 1.0)
WHITE  = unreal.LinearColor(0.878, 0.918, 1.000, 1.0)
MUTED  = unreal.LinearColor(0.168, 0.263, 0.420, 1.0)

WIDGET_PATHS = [
    "/Game/User_Interface/WBP_MainMenu",
    "/Game/User_Interface/WBP_BIMPlacementHUD",
    "/Game/User_Interface/WBP_BackButton",
    "/Game/User_Interface/WBP_GeoReadout",
    "/Game/User_Interface/WBP_VolumeReadout",
]

# Per-widget rules: widget_name (as shown in hierarchy) → {property: value}
# widget_name is checked as a substring match (case-insensitive)
# Font sizes are James's display-corrected values (+10 over nominal web sizes;
# 14pt reads too small on his setup, 24 is the readable floor). See feedback memory.
NAME_RULES = {
    # WBP_MainMenu
    "AppTitle":      {"color_and_opacity": WHITE,  "font_size": 46},
    "AppSubtitle":   {"color_and_opacity": MUTED,  "font_size": 26},
    "Phase1Button":  {"color_and_opacity": WHITE,  "font_size": 26},
    "Phase2Button":  {"color_and_opacity": WHITE,  "font_size": 26},
    # WBP_BIMPlacementHUD
    "InstructionText": {"color_and_opacity": WHITE, "font_size": 27},
    "StatusTag":       {"color_and_opacity": CYAN,  "font_size": 21},
    # WBP_BackButton
    "BackLabel":       {"color_and_opacity": WHITE, "font_size": 25},
    # WBP_GeoReadout
    "GpsLabel":        {"color_and_opacity": MUTED, "font_size": 22},
    "GpsCoords":       {"color_and_opacity": CYAN,  "font_size": 24},
    "AltText":         {"color_and_opacity": MUTED, "font_size": 22},
    # WBP_VolumeReadout
    "CutLabel":        {"color_and_opacity": CYAN,  "font_size": 22},
    "FillLabel":       {"color_and_opacity": CYAN,  "font_size": 22},
    "CutText":         {"color_and_opacity": WHITE, "font_size": 38},
    "FillText":        {"color_and_opacity": WHITE, "font_size": 38},
}


def set_font_size(text_block, size):
    try:
        font = text_block.get_editor_property("font")
        font.set_editor_property("size", size)
        text_block.set_editor_property("font", font)
        return True
    except Exception as e:
        unreal.log_warning("  font_size set failed: {}".format(e))
        return False


def set_color(text_block, color):
    try:
        slate_color = unreal.SlateColor()
        slate_color.set_editor_property("specified_color", color)
        text_block.set_editor_property("color_and_opacity", slate_color)
        return True
    except Exception as e:
        unreal.log_warning("  color set failed: {}".format(e))
        return False


def apply_rules(widget, rules):
    changed = False
    if "font_size" in rules:
        if set_font_size(widget, rules["font_size"]):
            changed = True
    if "color_and_opacity" in rules:
        if set_color(widget, rules["color_and_opacity"]):
            changed = True
    return changed


def process_widget_bp(path):
    wb = unreal.load_asset(path)
    if not wb:
        unreal.log_warning("[styles] not found: {}".format(path))
        return

    tree = wb.widget_tree
    widgets = tree.get_all_widgets()
    hits = 0
    for w in widgets:
        w_name = w.get_name()
        for key, rules in NAME_RULES.items():
            if key.lower() in w_name.lower() and isinstance(w, unreal.TextBlock):
                unreal.log_warning("[styles] {} → applying {} rules".format(w_name, list(rules.keys())))
                if apply_rules(w, rules):
                    hits += 1
    unreal.log_warning("[styles] {} — {} widgets updated".format(path.split("/")[-1], hits))


def run():
    for path in WIDGET_PATHS:
        process_widget_bp(path)

    try:
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        unreal.log_warning("[styles] saved.")
    except Exception as e:
        unreal.log_warning("[styles] save skipped: {}".format(e))


run()
