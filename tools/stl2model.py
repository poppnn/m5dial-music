#!/usr/bin/env python3
"""Convertit un STL en src/model.h (maillage allégé, en flash) pour la page 3D du M5Dial.

Usage : python tools/stl2model.py modele.stl [nb_triangles]   (défaut 3000)
Prérequis : pip install trimesh numpy scikit-image fast-simplification
Le STL doit avoir Z vers le haut (cas des modèles à imprimer). Sortie : x = longueur, y = haut, z = largeur,
centré, coordonnées entières dans [-1000, 1000] (int16), normale de chaque face en int8 (x127).

Les modèles d'impression sont souvent faits de centaines de morceaux ouverts que la décimation ne sait pas
réduire : on reconstruit d'abord une enveloppe fermée (voxels pleins -> marching cubes), on lisse les marches
d'escalier des voxels (Taubin, sans rétrécir), puis on décime.
"""
import sys
from pathlib import Path
import fast_simplification
import numpy as np
import trimesh

# Doivent rester égaux à CAR_PITCH / CAR_DIST dans main.cpp (sert à dimensionner le tableau des faces visibles).
PITCH, DIST = 0.28, 3500

src = sys.argv[1]
target = int(sys.argv[2]) if len(sys.argv) > 2 else 3000

m = trimesh.load(src, force="mesh")
vg = m.voxelized(m.extents.max() / 260).fill()   # 260 voxels sur la longueur : roues, montants, pare-chocs
shell = vg.marching_cubes
shell.apply_transform(vg.transform)
trimesh.smoothing.filter_taubin(shell, iterations=8)  # 8 : efface les marches, 20 arrondit trop les arêtes
vs, fs = fast_simplification.simplify(shell.vertices.astype(np.float32), shell.faces.astype(np.int32),
                                      target_count=target, agg=7)
m = trimesh.Trimesh(vs, fs)
m.fix_normals()  # triangles orientés vers l'extérieur : le rendu élimine les faces arrière
if not m.is_watertight:
    print("note : quelques arêtes ouvertes après décimation (sans effet visible au rendu)")

v = m.vertices[:, [0, 2, 1]] * [1, 1, -1]  # Z haut -> Y haut ; -Y pour garder le sens des triangles (repère direct)
v -= (v.min(0) + v.max(0)) / 2
v = np.round(v * (1000 / np.abs(v).max())).astype(np.int16)
f = m.faces.astype(np.uint16)
assert len(v) < 65535 and f.max() < len(v) and len(f) > 0

vf = v.astype(np.float64)
nrm = np.cross(vf[f[:, 1]] - vf[f[:, 0]], vf[f[:, 2]] - vf[f[:, 0]])
nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-9
n8 = np.round(nrm * 127).astype(np.int8)

# Nombre max de faces visibles sur un tour complet (même projection que drawCar) -> taille du tableau en RAM.
def visible(yaw):
    cy, sy, cp, sp = np.cos(yaw), np.sin(yaw), np.cos(PITCH), np.sin(PITCH)
    xr, zr = vf[:, 0] * cy + vf[:, 2] * sy, -vf[:, 0] * sy + vf[:, 2] * cy
    yc, zc = vf[:, 1] * cp - zr * sp, vf[:, 1] * sp + zr * cp
    k = DIST / (DIST - zc)
    X, Y = xr * k, -yc * k
    a, b, c = f[:, 0], f[:, 1], f[:, 2]
    cross = (X[b] - X[a]) * (Y[c] - Y[a]) - (Y[b] - Y[a]) * (X[c] - X[a])
    return int((cross < 0).sum())
vis_max = int(max(visible(y) for y in np.linspace(0, 2 * np.pi, 360)) * 1.05) + 8

out = Path(__file__).resolve().parent.parent / "src" / "model.h"
rows = lambda a: ",\n".join("  {" + ", ".join(str(int(x)) for x in r) + "}" for r in a)
out.write_text(
    "// Maillage de la page 3D, généré par tools/stl2model.py depuis " + Path(src).name + "\n"
    "// x = longueur, y = haut, z = largeur ; entiers dans [-1000, 1000]. En flash (const).\n"
    "#pragma once\n#include <stdint.h>\n\n"
    f"static const uint16_t MODEL_NV = {len(v)}, MODEL_NT = {len(f)};\n"
    f"static const uint16_t MODEL_VIS_MAX = {vis_max};  // faces visibles au plus sur un tour (+5 %)\n"
    f"static const int16_t model_v[][3] = {{\n{rows(v)}\n}};\n"
    f"static const uint16_t model_t[][3] = {{\n{rows(f)}\n}};\n"
    f"static const int8_t model_n[][3] = {{  // normale de chaque face, x127\n{rows(n8)}\n}};\n",
    encoding="utf-8",
)
print(f"{out} : {len(v)} sommets, {len(f)} triangles, {vis_max} visibles max, "
      f"{len(v) * 6 + len(f) * 9} octets en flash")
