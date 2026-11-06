#!/usr/bin/env python3
"""Convertit un PNG/BMP en src/logo.h (RGB565, en flash) pour l'écran de démarrage du M5Dial.

Usage : python tools/png2logo.py mon_logo.png [largeur_max hauteur_max]   (défaut 150 x 130)
Prérequis : pip install pillow
La transparence est posée sur du noir (fond de l'écran). L'image est réduite en gardant ses proportions.
"""
import sys
from pathlib import Path
from PIL import Image

src = sys.argv[1]
max_w, max_h = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) > 3 else (150, 130)

img = Image.open(src).convert("RGBA")
img.thumbnail((max_w, max_h), Image.LANCZOS)
img = Image.alpha_composite(Image.new("RGBA", img.size, (0, 0, 0, 255)), img).convert("RGB")
w, h = img.size
raw = img.tobytes()
px = [((raw[i] >> 3) << 11) | ((raw[i + 1] >> 2) << 5) | (raw[i + 2] >> 3) for i in range(0, len(raw), 3)]
assert len(px) == w * h and all(p <= 0xFFFF for p in px)

rows = ",\n".join("  " + ", ".join(f"0x{p:04X}" for p in px[i:i + 12]) for i in range(0, len(px), 12))
out = Path(__file__).resolve().parent.parent / "src" / "logo.h"
out.write_text(
    "// Logo de l'écran de démarrage, RGB565 en flash (const => .rodata, ne prend pas de RAM).\n"
    f"// Généré par tools/png2logo.py depuis {Path(src).name}\n"
    "#pragma once\n#include <stdint.h>\n#include <pgmspace.h>\n\n"
    f"static const uint16_t LOGO_W = {w}, LOGO_H = {h};\n"
    f"static const uint16_t logo_data[] PROGMEM = {{\n{rows}\n}};\n",
    encoding="utf-8",
)
print(f"{out} : {w} x {h} px, {w * h * 2} octets en flash")
