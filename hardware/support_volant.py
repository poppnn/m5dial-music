#!/usr/bin/env python3
"""Support imprimable 3D : M5Dial sur volant GT2i Race 350 mm (tulipage 75 mm).

Le M5Dial (rond, Ø 51 mm) se loge dans une coupelle placée dans l'ouverture entre la branche du bas
et la branche droite. Deux bras se vissent dans les trous ovales intérieurs de ces branches (vis M5),
par l'arrière du volant : pattes plaquées sous la branche, vis passée de l'avant, écrou nylstop derrière.

Repère (vue de face, conducteur devant le volant) : X vers la droite, Y vers le haut, Z vers le conducteur,
origine = centre du moyeu, Z = 0 = face arrière des branches au niveau des trous de fixation.

Usage : pip install manifold3d trimesh
        python hardware/support_volant.py       -> écrit hardware/support_volant.stl
Les cotes par défaut sont estimées sur une photo : mesure les 5 valeurs marquées MESURER avant d'imprimer.
"""
from pathlib import Path
from manifold3d import Manifold
import numpy as np
import trimesh

SEG = 96  # finesse des cercles

# ---- M5Dial et coupelle ----
DIAL_D = 51.4          # Ø intérieur de la coupelle (M5Dial Ø 51,0 + 0,4 de jeu, des nervures serrent)
WALL = 2.5             # épaisseur de paroi
FLOOR = 3.0            # épaisseur du fond
CUP_H = 12.0           # hauteur de paroi : ne tient que le bas du boîtier, la bague reste libre
FLOOR_HOLE_D = 40.0    # ouverture du fond : connecteurs arrière (alim, Port A/B, IMU) et câbles
NOTCH_W = 16.0         # encoche latérale pleine hauteur pour le câble USB-C
NOTCH_ANGLE = -90.0    # direction de l'encoche (deg, 0 = droite, -90 = vers le bas du volant)
RIBS, RIB = 6, 0.4     # nervures d'écrasement : serrage du M5Dial sans dépendre de la précision de l'imprimante

# ---- Position (MESURER) ----
DEVICE_XY = (78.6, -73.8)   # MESURER : centre du carré rouge par rapport au centre du moyeu (mm)

# Pattes de fixation, une par branche. xy = centre du trou ovale (MESURER), radial = direction de la branche
# (deg), tilt = pente de la branche vers le conducteur (MESURER, tulipage), dz = écart de profondeur (MESURER).
PADS = [
    dict(name="droite", xy=(71.7, 3.3), radial=5.0, tilt=30.0, dz=0.0),
    dict(name="bas", xy=(0.0, -70.4), radial=-90.0, tilt=30.0, dz=0.0),
]
PAD_L, PAD_W, PAD_T = 26.0, 22.0, 5.0   # patte : longueur (le long de la branche), largeur, épaisseur
SLOT_W, SLOT_L = 5.5, 12.0              # lumière M5, ±3 mm de réglage le long de la branche
ARM_W, ARM_T = 16.0, 6.0                # section des bras côté coupelle (PETG/ASA : rigide, tient au soleil)


def pad_place(m: Manifold, p: dict) -> Manifold:
    """Incline la patte comme la branche (l'extérieur remonte vers le conducteur) puis la positionne."""
    return m.rotate((0, -p["tilt"], 0)).rotate((0, 0, p["radial"])).translate((*p["xy"], p["dz"]))


def slot(length: float, width: float, height: float) -> Manifold:
    end = Manifold.cylinder(height, width / 2, circular_segments=32, center=True)
    return Manifold.batch_hull([end.translate((-(length - width) / 2, 0, 0)),
                                end.translate(((length - width) / 2, 0, 0))])


def build() -> Manifold:
    r_in, r_out = DIAL_D / 2, DIAL_D / 2 + WALL
    cx, cy = DEVICE_XY
    body = Manifold.cylinder(FLOOR + CUP_H, r_out, circular_segments=SEG).translate((cx, cy, -FLOOR))

    for p in PADS:
        pad = pad_place(Manifold.cube((PAD_L, PAD_W, PAD_T), center=True).translate((0, 0, -PAD_T / 2)), p)
        ang = np.degrees(np.arctan2(p["xy"][1] - cy, p["xy"][0] - cx))
        root = (Manifold.cube((6, ARM_W, ARM_T), center=True).translate((r_out - 3, 0, -ARM_T / 2))
                .rotate((0, 0, ang)).translate((cx, cy, 0)))
        body += Manifold.batch_hull([pad, root])  # bras plein et effilé, sans angle vif

    body -= Manifold.cylinder(CUP_H + 1, r_in, circular_segments=SEG).translate((cx, cy, 0))
    body -= Manifold.cylinder(FLOOR + CUP_H + 20, FLOOR_HOLE_D / 2, circular_segments=SEG).translate((cx, cy, -FLOOR - 10))
    body -= (Manifold.cube((r_out + 2, NOTCH_W, CUP_H + 1)).translate((0, -NOTCH_W / 2, 0))
             .rotate((0, 0, NOTCH_ANGLE)).translate((cx, cy, 0)))
    for p in PADS:
        body -= pad_place(slot(SLOT_L, SLOT_W, 40), p)

    rib = Manifold.cylinder(CUP_H, 0.8, circular_segments=16)
    for i in range(RIBS):
        a = NOTCH_ANGLE + 360 * (i + 0.5) / RIBS  # décalées de l'encoche
        d = r_in + 0.8 - RIB
        body += rib.translate((cx + d * np.cos(np.radians(a)), cy + d * np.sin(np.radians(a)), 0))
    return body


if __name__ == "__main__":
    mesh = build().to_mesh()
    tm = trimesh.Trimesh(mesh.vert_properties[:, :3], mesh.tri_verts)
    assert tm.is_watertight and tm.volume > 0, "maillage invalide"
    out = Path(__file__).with_suffix(".stl")
    tm.export(out)
    lo, hi = tm.bounds
    print(f"{out} : {tm.volume / 1000:.1f} cm3, encombrement {hi[0]-lo[0]:.0f} x {hi[1]-lo[1]:.0f} x {hi[2]-lo[2]:.0f} mm")
