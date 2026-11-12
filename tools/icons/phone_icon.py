"""Génère src/phone_icon.h : combiné de téléphone (icône "call" de Material Icons, Google, licence Apache 2.0)
en masque d'opacité 8 bits (0 = transparent, 255 = plein), lissé, pour l'écran d'appel.
PHONE_ACCEPT : décroché (rond vert). PHONE_DECLINE : le même tourné de 135°, posé à l'horizontale (rond rouge).

    python tools/icons/phone_icon.py      (Pillow requis)
"""
import re
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 30      # pixels à l'écran
SS = 16        # suréchantillonnage pour le lissage
PATH = ("M20.01 15.38c-1.23 0-2.42-.2-3.53-.56-.35-.12-.74-.03-1.01.24l-1.57 1.97c-2.83-1.35-5.48-3.9-6.89-6.83"
        "l1.95-1.66c.27-.28.35-.67.24-1.02-.37-1.11-.56-2.3-.56-3.53 0-.54-.45-.99-.99-.99H4.19C3.65 3 3 3.24 3 3.99"
        " 3 13.28 10.73 21 20.01 21c.71 0 .99-.63.99-1.18v-3.45c0-.54-.45-.99-.99-.99z")


def flatten(d):
    """Chemin SVG (M L H V C Z, absolu ou relatif) -> liste de points (courbes découpées en 12 segments)."""
    toks = re.findall(r"[MLHVCZmlhvcz]|-?\d*\.?\d+(?:e-?\d+)?", d)
    pts, x, y, i, cmd = [], 0.0, 0.0, 0, None
    while i < len(toks):
        if re.match(r"[A-Za-z]", toks[i]):
            cmd = toks[i]
            i += 1
            if cmd in "Zz":
                continue
        rel = cmd.islower()
        n = {"m": 2, "l": 2, "h": 1, "v": 1, "c": 6}[cmd.lower()]
        v = [float(t) for t in toks[i:i + n]]
        i += n
        c = cmd.lower()
        if c in "ml":
            x, y = (x + v[0], y + v[1]) if rel else (v[0], v[1])
            pts.append((x, y))
            if c == "m":
                cmd = "l" if rel else "L"  # coordonnées suivantes d'un M = lignes
        elif c == "h":
            x = x + v[0] if rel else v[0]
            pts.append((x, y))
        elif c == "v":
            y = y + v[0] if rel else v[0]
            pts.append((x, y))
        else:
            ox, oy = (x, y) if rel else (0.0, 0.0)
            p0, p1, p2, p3 = (x, y), (ox + v[0], oy + v[1]), (ox + v[2], oy + v[3]), (ox + v[4], oy + v[5])
            for k in range(1, 13):
                t = k / 12
                a, b, cc, dd = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
                pts.append((a * p0[0] + b * p1[0] + cc * p2[0] + dd * p3[0], a * p0[1] + b * p1[1] + cc * p2[1] + dd * p3[1]))
            x, y = p3
    return pts


def mask(rotate):
    big = Image.new("L", (24 * SS, 24 * SS), 0)
    ImageDraw.Draw(big).polygon([(px * SS, py * SS) for px, py in flatten(PATH)], fill=255)
    big = big.rotate(rotate, resample=Image.BICUBIC, center=(12 * SS, 12 * SS))
    big = big.crop(big.getbbox())
    s = (SIZE + (4 if rotate else 0)) / max(big.size)  # couché : un peu plus large, comme sur l'iPhone
    return big.resize((round(big.width * s), round(big.height * s)), Image.LANCZOS)


def c_array(name, im):
    px = im.load()
    rows = [", ".join(str(px[x, r]) for x in range(im.width)) for r in range(im.height)]
    return (f"#define {name}_W {im.width}\n#define {name}_H {im.height}\n"
            f"static const uint8_t {name}[] = {{\n  " + ",\n  ".join(rows) + "};\n")


if __name__ == "__main__":
    accept, decline = mask(0), mask(-135)
    out = Path(__file__).resolve().parents[2] / "src" / "phone_icon.h"
    out.write_text("// Généré par tools/icons/phone_icon.py : icône \"call\" de Material Icons (Google, Apache 2.0),\n"
                   "// masque d'opacité 8 bits. Ne pas modifier à la main.\n#pragma once\n#include <stdint.h>\n\n"
                   + c_array("PHONE_ACCEPT", accept) + "\n" + c_array("PHONE_DECLINE", decline), encoding="utf-8")
    print(out, accept.size, decline.size)
