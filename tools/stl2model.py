#!/usr/bin/env python3
"""Convertit un STL en src/model.h (maillage allégé, en flash) pour la page 3D du M5Dial.

Usage : python tools/stl2model.py modele.stl [nb_triangles]   (défaut 1200)
Prérequis : pip install trimesh numpy scikit-image fast-simplification
Le STL doit avoir Z vers le haut (cas des modèles à imprimer). Sortie : x = longueur, y = haut, z = largeur,
centré, coordonnées entières dans [-1000, 1000] (int16).

Les modèles d'impression sont souvent faits de centaines de morceaux ouverts que la décimation ne sait pas
réduire : on reconstruit d'abord une enveloppe fermée (voxels pleins -> marching cubes), puis on décime.
"""
import sys
from pathlib import Path
import fast_simplification
import numpy as np
import trimesh

src = sys.argv[1]
target = int(sys.argv[2]) if len(sys.argv) > 2 else 1200

m = trimesh.load(src, force="mesh")
vg = m.voxelized(m.extents.max() / 145).fill()   # ~145 voxels sur la longueur : détails fins inutiles en 240 px
shell = vg.marching_cubes
shell.apply_transform(vg.transform)
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

out = Path(__file__).resolve().parent.parent / "src" / "model.h"
rows = lambda a: ",\n".join("  {" + ", ".join(str(int(x)) for x in r) + "}" for r in a)
out.write_text(
    "// Maillage de la page 3D, généré par tools/stl2model.py depuis " + Path(src).name + "\n"
    "// x = longueur, y = haut, z = largeur ; entiers dans [-1000, 1000]. En flash (const).\n"
    "#pragma once\n#include <stdint.h>\n\n"
    f"static const uint16_t MODEL_NV = {len(v)}, MODEL_NT = {len(f)};\n"
    f"static const int16_t model_v[][3] = {{\n{rows(v)}\n}};\n"
    f"static const uint16_t model_t[][3] = {{\n{rows(f)}\n}};\n",
    encoding="utf-8",
)
print(f"{out} : {len(v)} sommets, {len(f)} triangles, {len(v) * 6 + len(f) * 6} octets en flash")
