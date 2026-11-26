"""Génère src/ui_fonts.h : polices lissées Manrope (VLW, alpha 8 bits) pour l'interface, une par style.

Manrope (Google Fonts, SIL Open Font License 1.1, voir OFL.txt) en police variable :
    curl -L -o Manrope.ttf "https://raw.githubusercontent.com/google/fonts/main/ofl/manrope/Manrope%5Bwght%5D.ttf"
    python tools/fonts/make_fonts.py Manrope.ttf      (Pillow requis)

Jeux de caractères réduits (la flash est presque pleine) : ASCII + Latin-1 + quelques signes pour le texte, chiffres
seuls pour les grands chiffres. Les chiffres ont tous la même chasse (alignés comme des chiffres tabulaires). Un texte
hors de ces jeux (japonais, cyrillique...) est affiché avec la police efont (ui.cpp, uiFont).
"""
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

TEXT = [c for c in range(0x20, 0x7F)] + [c for c in range(0xA0, 0x100) if c != 0xAD] + \
       [0x152, 0x153, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC]
ASCII = [c for c in range(0x20, 0x7F)] + [0xB0, 0xB7, 0x2013, 0x2014, 0x2026]
DIGITS = [ord(c) for c in " .0123456789:"]

# nom, taille (px), graisse, espacement ajouté (px), caractères
STYLES = [
    ("CAPTION", 11, 700, 1, TEXT),     # légendes en capitales espacées
    ("MICRO", 11, 600, 0, ASCII),      # temps, petites indications
    ("SMALL", 13, 600, 0, TEXT),       # lignes de la page Système
    ("BODY", 15, 500, 0, TEXT),        # artiste, lignes des listes, message
    ("STRONG", 16, 700, 0, TEXT),      # ligne choisie d'une liste
    ("TITLE", 21, 700, 0, TEXT),       # titre, préréglage LED, appelant
    ("FIGURE", 22, 600, 0, DIGITS),    # centièmes
    ("DISPLAY", 46, 700, 0, DIGITS + [ord("%")]),  # chrono, volume, téléchargement
    ("DISPLAY_XL", 60, 800, 0, DIGITS),            # heure
]


def vlw(path, size, weight, spacing, chars):
    f = ImageFont.truetype(path, size, layout_engine=ImageFont.Layout.BASIC)
    f.set_variation_by_axes([weight])
    digit_adv = max(f.getlength(str(d)) for d in range(10))
    glyphs = []
    for c in sorted(set(chars)):
        ch = chr(c)
        adv = f.getlength(ch)
        x0, y0, x1, y1 = f.getbbox(ch, anchor="ls")
        shift = 0
        if ch.isdigit():  # chasse fixe : chiffre centré dans la largeur du plus large
            shift = round((digit_adv - adv) / 2)
            adv = digit_adv
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        bmp = b""
        if w and h and ch != " ":
            im = Image.new("L", (w, h), 0)
            ImageDraw.Draw(im).text((-x0, -y0), ch, font=f, fill=255, anchor="ls")
            bmp = im.tobytes()
        else:
            w = h = 0
        glyphs.append((c, h, w, round(adv) + spacing, -y0 if h else 0, x0 + shift if w else 0, bmp))
    asc = -f.getbbox("d", anchor="ls")[1]
    desc = f.getbbox("p", anchor="ls")[3]
    out = struct.pack(">6i", len(glyphs), 11, size, 0, asc, desc)
    for c, h, w, adv, dy, dx, _ in glyphs:
        out += struct.pack(">7i", c, h, w, adv, dy, dx, 0)
    for g in glyphs:
        out += g[6]
    return out


def main():
    ttf = sys.argv[1] if len(sys.argv) > 1 else "Manrope.ttf"
    lines = ["// Généré par tools/fonts/make_fonts.py : Manrope (SIL OFL 1.1, tools/fonts/OFL.txt), VLW lissé.",
             "// Ne pas modifier à la main.", "#pragma once", "#include <stdint.h>", ""]
    total = 0
    for name, size, weight, spacing, chars in STYLES:
        data = vlw(ttf, size, weight, spacing, chars)
        total += len(data)
        lines.append(f"// {name} : {size} px, graisse {weight}, {len(set(chars))} caractères, {len(data)} octets")
        lines.append(f"static const uint8_t FONT_{name}[] = {{")
        for i in range(0, len(data), 24):
            lines.append("  " + ", ".join(str(b) for b in data[i:i + 24]) + ",")
        lines.append("};")
        lines.append("")
    Path(__file__).resolve().parents[2].joinpath("src", "ui_fonts.h").write_text("\n".join(lines))
    print(f"src/ui_fonts.h : {total} octets de polices")


if __name__ == "__main__":
    main()
