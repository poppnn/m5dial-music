"""Fusion 360 : support M5Dial pour volant GT2i Race 350 mm (même géométrie que hardware/support_volant.py).

Lancer : Utilitaires > Compléments > Scripts > "+" > dossier SupportVolant > Exécuter.
Au premier lancement, les cotes sont créées en paramètres utilisateur (Modifier > Changer les paramètres).
Pour appliquer de nouvelles cotes : modifie-les là, puis relance le script (il remplace le composant).

Repère : X droite, Y haut (vue de face), Z vers le conducteur, origine = centre du moyeu,
Z = 0 = face arrière des branches au niveau des trous de fixation.
"""
import math
import traceback

import adsk.core
import adsk.fusion

NAME = "Support M5Dial"

# (nom, valeur par défaut, unité, commentaire). Les cotes MESURER viennent d'une photo : à vérifier.
PARAMS = [
    ("DIAL_D", 51.4, "mm", "Ø intérieur coupelle (M5Dial 51 + jeu)"),
    ("WALL", 2.5, "mm", "Épaisseur paroi"),
    ("FLOOR", 3.0, "mm", "Épaisseur fond"),
    ("CUP_H", 12.0, "mm", "Hauteur paroi (la bague reste libre)"),
    ("FLOOR_HOLE_D", 40.0, "mm", "Ouverture du fond (connecteurs, IMU)"),
    ("NOTCH_W", 16.0, "mm", "Encoche câble USB-C"),
    ("NOTCH_ANGLE", -90.0, "deg", "Direction encoche (0 = droite, -90 = bas)"),
    ("DEV_X", 78.6, "mm", "MESURER : centre M5Dial, X"),
    ("DEV_Y", -73.8, "mm", "MESURER : centre M5Dial, Y"),
    ("R_X", 71.7, "mm", "MESURER : trou branche droite, X"),
    ("R_Y", 3.3, "mm", "MESURER : trou branche droite, Y"),
    ("R_RADIAL", 5.0, "deg", "Direction branche droite"),
    ("R_TILT", 30.0, "deg", "MESURER : pente branche droite"),
    ("R_DZ", 0.0, "mm", "MESURER : décalage profondeur droite"),
    ("B_X", 0.0, "mm", "MESURER : trou branche bas, X"),
    ("B_Y", -70.4, "mm", "MESURER : trou branche bas, Y"),
    ("B_RADIAL", -90.0, "deg", "Direction branche bas"),
    ("B_TILT", 30.0, "deg", "MESURER : pente branche bas"),
    ("B_DZ", 0.0, "mm", "MESURER : décalage profondeur bas"),
    ("SPOKE_W", 32.0, "mm", "Largeur branche (zone où rien ne dépasse au-dessus)"),
    ("PAD_L", 26.0, "mm", "Patte : longueur le long de la branche"),
    ("PAD_W", 22.0, "mm", "Patte : largeur"),
    ("PAD_T", 5.0, "mm", "Patte : épaisseur"),
    ("SLOT_W", 5.5, "mm", "Lumière M5 : largeur"),
    ("SLOT_L", 12.0, "mm", "Lumière M5 : longueur"),
    ("ARM_W", 16.0, "mm", "Bras : largeur"),
    ("ARM_T", 6.0, "mm", "Bras : épaisseur"),
    ("RIBS", 6.0, "", "Nombre de nervures de serrage"),
    ("RIB", 0.4, "mm", "Dépassement des nervures"),
]

NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
CUT = adsk.fusion.FeatureOperations.CutFeatureOperation


def read_params(design):
    """Crée les paramètres manquants et renvoie les valeurs en mm / degrés (l'API stocke cm et radians)."""
    out = {}
    ups = design.userParameters
    for name, default, unit, comment in PARAMS:
        p = ups.itemByName(name)
        if not p:
            p = ups.add(name, adsk.core.ValueInput.createByString(f"{default} {unit}".strip()), unit, comment)
        v = p.value
        out[name] = v * 10 if unit == "mm" else math.degrees(v) if unit == "deg" else v
    return out


class Builder:
    def __init__(self, comp):
        self.comp = comp
        self.f = comp.features

    def _pt(self, x, y):
        return adsk.core.Point3D.create(x / 10, y / 10, 0)

    def sketch(self):
        return self.comp.sketches.add(self.comp.xYConstructionPlane)

    def poly(self, pts):
        sk = self.sketch()
        lines = sk.sketchCurves.sketchLines
        first = prev = lines.addByTwoPoints(self._pt(*pts[0]), self._pt(*pts[1]))
        for p in pts[2:]:
            prev = lines.addByTwoPoints(prev.endSketchPoint, self._pt(*p))
        lines.addByTwoPoints(prev.endSketchPoint, first.startSketchPoint)
        return sk

    def circles(self, centers, r):
        sk = self.sketch()
        for c in centers:
            sk.sketchCurves.sketchCircles.addByCenterRadius(self._pt(*c), r / 10)
        return sk

    def rect(self, cx, cy, length, width, ang):
        """Rectangle centré, côté `length` orienté selon `ang` (deg)."""
        u = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
        n = (-u[1], u[0])
        a, b = length / 2, width / 2
        return self.poly([(cx + sa * a * u[0] + sb * b * n[0], cy + sa * a * u[1] + sb * b * n[1])
                          for sa, sb in ((1, 1), (-1, 1), (-1, -1), (1, -1))])

    def extrude(self, sk, z0, h, op, bodies=None):
        """Extrude tous les profils de l'esquisse de z0 à z0 + h (mm)."""
        profs = adsk.core.ObjectCollection.create()
        for i in range(sk.profiles.count):
            profs.add(sk.profiles.item(i))
        ext = self.f.extrudeFeatures
        inp = ext.createInput(profs, op)
        inp.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(adsk.core.ValueInput.createByReal(h / 10)),
                             adsk.fusion.ExtentDirections.PositiveExtentDirection)
        if z0:
            inp.startExtent = adsk.fusion.OffsetStartDefinition.create(adsk.core.ValueInput.createByReal(z0 / 10))
        if bodies:
            inp.participantBodies = bodies
        return ext.add(inp)

    def tilt(self, body, x, y, radial, tilt, dz):
        """Incline comme la branche (l'extérieur remonte vers le conducteur) autour d'un axe passant par le trou."""
        r = math.radians(radial)
        m = adsk.core.Matrix3D.create()
        m.setToRotation(math.radians(-tilt), adsk.core.Vector3D.create(-math.sin(r), math.cos(r), 0),
                        adsk.core.Point3D.create(x / 10, y / 10, 0))
        tr = m.translation  # rotation d'abord, puis décalage en profondeur
        tr.z += dz / 10
        m.translation = tr
        coll = adsk.core.ObjectCollection.create()
        coll.add(body)
        mf = self.f.moveFeatures
        if hasattr(mf, "createInput2"):
            inp = mf.createInput2(coll)
            inp.defineAsFreeMove(m)
        else:
            inp = mf.createInput(coll, m)
        mf.add(inp)

    def combine(self, target, tool, op):
        coll = adsk.core.ObjectCollection.create()
        coll.add(tool)
        inp = self.f.combineFeatures.createInput(target, coll)
        inp.operation = op
        inp.isKeepToolBodies = False
        self.f.combineFeatures.add(inp)


def build(comp, P):
    b = Builder(comp)
    cx, cy = P["DEV_X"], P["DEV_Y"]
    r_in, r_out = P["DIAL_D"] / 2, P["DIAL_D"] / 2 + P["WALL"]

    cup = b.extrude(b.circles([(cx, cy)], r_out), -P["FLOOR"], P["FLOOR"] + P["CUP_H"], NEW).bodies.item(0)
    cup.name = NAME

    for k in ("R", "B"):
        x, y, rad, til, dz = (P[k + "_X"], P[k + "_Y"], P[k + "_RADIAL"], P[k + "_TILT"], P[k + "_DZ"])

        def tilted(sk, z0, h):
            body = b.extrude(sk, z0, h, NEW).bodies.item(0)
            b.tilt(body, x, y, rad, til, dz)
            return body

        # Bras plat de la coupelle jusqu'au trou, puis patte inclinée plaquée sous la branche
        ang = math.degrees(math.atan2(y - cy, x - cx))
        u = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
        n, w = (-u[1], u[0]), P["ARM_W"] / 2
        root = (cx + u[0] * (r_out - 3), cy + u[1] * (r_out - 3))
        arm = b.extrude(b.poly([(root[0] + n[0] * w, root[1] + n[1] * w), (x + n[0] * w, y + n[1] * w),
                                (x - n[0] * w, y - n[1] * w), (root[0] - n[0] * w, root[1] - n[1] * w)]),
                        -P["ARM_T"], P["ARM_T"], NEW).bodies.item(0)
        b.combine(cup, arm, JOIN)
        b.combine(cup, tilted(b.rect(x, y, P["PAD_L"], P["PAD_W"], rad), -P["PAD_T"], P["PAD_T"]), JOIN)
        # Rien ne doit dépasser au-dessus de la face arrière de la branche
        b.combine(cup, tilted(b.rect(x, y, 70, P["SPOKE_W"], rad), 0, 40), CUT)
        # Lumière M5 le long de la branche
        d = (P["SLOT_L"] - P["SLOT_W"]) / 2
        ur = (math.cos(math.radians(rad)), math.sin(math.radians(rad)))
        sk = b.circles([(x + ur[0] * d, y + ur[1] * d), (x - ur[0] * d, y - ur[1] * d)], P["SLOT_W"] / 2)
        slot = b.extrude(sk, -30, 60, NEW).bodies.item(0)
        bar = b.extrude(b.rect(x, y, 2 * d, P["SLOT_W"], rad), -30, 60, NEW).bodies.item(0)
        b.combine(slot, bar, JOIN)
        b.tilt(slot, x, y, rad, til, dz)
        b.combine(cup, slot, CUT)

    # Logement, ouverture du fond, encoche USB-C
    b.extrude(b.circles([(cx, cy)], r_in), 0, P["CUP_H"] + 1, CUT, [cup])
    b.extrude(b.circles([(cx, cy)], P["FLOOR_HOLE_D"] / 2), -P["FLOOR"] - 10, P["FLOOR"] + 20, CUT, [cup])
    na = math.radians(P["NOTCH_ANGLE"])
    L = r_out + 2
    b.extrude(b.rect(cx + math.cos(na) * L / 2, cy + math.sin(na) * L / 2, L, P["NOTCH_W"], P["NOTCH_ANGLE"]),
              0, P["CUP_H"] + 1, CUT, [cup])

    # Nervures de serrage, décalées de l'encoche
    n_ribs, d = int(round(P["RIBS"])), r_in + 0.8 - P["RIB"]
    if n_ribs:
        centers = [(cx + d * math.cos(na + 2 * math.pi * (i + 0.5) / n_ribs),
                    cy + d * math.sin(na + 2 * math.pi * (i + 0.5) / n_ribs)) for i in range(n_ribs)]
        b.extrude(b.circles(centers, 0.8), 0, P["CUP_H"], JOIN, [cup])
    return cup


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        design = adsk.fusion.Design.cast(app.activeProduct)
        if not design:
            ui.messageBox("Ouvre d'abord un design Fusion.")
            return
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        P = read_params(design)
        root = design.rootComponent
        for occ in [root.occurrences.item(i) for i in range(root.occurrences.count)]:
            if occ.component.name == NAME:
                occ.deleteMe()
        occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        occ.component.name = NAME
        cup = build(occ.component, P)
        vol = cup.physicalProperties.volume  # cm3
        ui.messageBox(f"{NAME} créé : {vol:.1f} cm3.\nCotes : Modifier > Changer les paramètres, puis relance le script.")
    except Exception:
        if ui:
            ui.messageBox("Erreur :\n" + traceback.format_exc())
