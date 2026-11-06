"""Fusion 360 : support M5Dial pour volant GT2i Race 350 mm (tulipage 75 mm). Source de hardware/support_volant.stl.

Lancer : Utilitaires > Compléments > Scripts > "+" > dossier SupportVolant > Exécuter.
Au premier lancement, les cotes sont créées en paramètres utilisateur (Modifier > Changer les paramètres).
Pour appliquer de nouvelles cotes : modifie-les là, puis relance le script (il remplace le groupe de timeline).

Pièce optimisée pour l'impression : une seule face plane posée sur le plateau, aucun support nécessaire.
- M5Dial collé (adhésif VHB) sur un anneau fin du même diamètre, sans paroi : invisible derrière le boîtier.
- Pattes en coin : dessus incliné comme la branche, dessous plat sur le plateau ; lamage d'appui de
  l'écrou perpendiculaire à la vis.

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
    ("DIAL_D", 51.0, "mm", "Ø extérieur de l'anneau = Ø du M5Dial (caché derrière)"),
    ("RING_T", 3.0, "mm", "Épaisseur de l'anneau"),
    ("FLOOR_HOLE_D", 40.0, "mm", "Ø intérieur de l'anneau (connecteurs, IMU, câbles)"),
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
    ("PAD_L", 20.0, "mm", "Patte : longueur le long de la branche"),
    ("PAD_W", 34.0, "mm", "Patte : largeur (> SPOKE_W, sinon le bras s'amincit sous la branche)"),
    ("PAD_MIN", 3.0, "mm", "Patte : épaisseur mini (bout bas du coin)"),
    ("SEAT_T", 4.0, "mm", "Matière serrée entre branche et écrou"),
    ("SEAT_D", 12.0, "mm", "Ø du lamage (rondelle M5 + écrou)"),
    ("SLOT_W", 5.5, "mm", "Lumière M5 : largeur"),
    ("SLOT_L", 12.0, "mm", "Lumière M5 : longueur"),
    ("ARM_W", 16.0, "mm", "Bras : largeur"),
    ("ARM_T", 6.0, "mm", "Bras : épaisseur"),
]
OBSOLETE = ["WALL", "FLOOR", "CUP_H", "NOTCH_W", "NOTCH_ANGLE", "RIBS", "RIB", "PAD_T"]  # ancienne coupelle

NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
CUT = adsk.fusion.FeatureOperations.CutFeatureOperation


def read_params(design):
    """Crée les paramètres manquants et renvoie les valeurs en mm / degrés (l'API stocke cm et radians)."""
    out = {}
    ups = design.userParameters
    for name in OBSOLETE:
        p = ups.itemByName(name)
        if p:
            p.deleteMe()
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

    def poly(self, pts, sk=None):
        sk = sk or self.sketch()
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

    def rect(self, cx, cy, length, width, ang, sk=None):
        """Rectangle centré, côté `length` orienté selon `ang` (deg)."""
        u = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
        n = (-u[1], u[0])
        a, b = length / 2, width / 2
        return self.poly([(cx + sa * a * u[0] + sb * b * n[0], cy + sa * a * u[1] + sb * b * n[1])
                          for sa, sb in ((1, 1), (-1, 1), (-1, -1), (1, -1))], sk)

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

    def solid(self, sk, z0, h):
        """Nouveau corps (les profils de l'esquisse doivent se toucher, sinon Fusion crée plusieurs corps)."""
        bodies = self.extrude(sk, z0, h, NEW).bodies
        assert bodies.count == 1, f"{bodies.count} corps au lieu d'un"
        return bodies.item(0)

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
    r_out = P["DIAL_D"] / 2
    # Plan du plateau : sous le bout le plus bas des coins, avec PAD_MIN de matière
    z_b = min(P[k + "_DZ"] - P["PAD_L"] / 2 * math.sin(math.radians(P[k + "_TILT"])) for k in ("R", "B")) - P["PAD_MIN"]

    cup = b.solid(b.circles([(cx, cy)], r_out), z_b, P["RING_T"])
    cup.name = NAME

    for k in ("R", "B"):
        x, y, rad, til, dz = (P[k + "_X"], P[k + "_Y"], P[k + "_RADIAL"], P[k + "_TILT"], P[k + "_DZ"])

        def tilted(sk, z0, h):
            body = b.solid(sk, z0, h)
            b.tilt(body, x, y, rad, til, dz)
            return body

        # Bras posé sur le plateau, de l'anneau jusqu'au trou, puis coin : dessus = face arrière de la branche
        ang = math.degrees(math.atan2(y - cy, x - cx))
        u = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
        n, w = (-u[1], u[0]), P["ARM_W"] / 2
        root = (cx + u[0] * (r_out - 3), cy + u[1] * (r_out - 3))
        arm = b.solid(b.poly([(root[0] + n[0] * w, root[1] + n[1] * w), (x + n[0] * w, y + n[1] * w),
                              (x - n[0] * w, y - n[1] * w), (root[0] - n[0] * w, root[1] - n[1] * w)]),
                      z_b, P["ARM_T"])
        b.combine(cup, arm, JOIN)
        b.combine(cup, tilted(b.rect(x, y, P["PAD_L"], P["PAD_W"], rad), -40, 40), JOIN)  # rogné au plateau plus bas
        # Rien ne doit dépasser au-dessus de la face arrière de la branche
        b.combine(cup, tilted(b.rect(x, y, 70, P["SPOKE_W"], rad), 0, 40), CUT)
        # Lumière M5 le long de la branche
        d = (P["SLOT_L"] - P["SLOT_W"]) / 2
        ur = (math.cos(math.radians(rad)), math.sin(math.radians(rad)))
        sk = b.circles([(x + ur[0] * d, y + ur[1] * d), (x - ur[0] * d, y - ur[1] * d)], P["SLOT_W"] / 2)
        b.combine(cup, tilted(b.rect(x, y, 2 * d, P["SLOT_W"], rad, sk), -30, 60), CUT)  # 2 cercles + barre = 1 corps
        # Lamage par-dessous : appui de l'écrou perpendiculaire à la vis (petit pont, sans support)
        b.combine(cup, tilted(b.circles([(x, y)], P["SEAT_D"] / 2), -40, 40 - P["SEAT_T"]), CUT)

    # Assise plane du M5Dial (rien ne dépasse de l'anneau sous le boîtier), passage des connecteurs
    b.extrude(b.circles([(cx, cy)], r_out), z_b + P["RING_T"], 60, CUT, [cup])
    b.extrude(b.circles([(cx, cy)], P["FLOOR_HOLE_D"] / 2), z_b - 1, 60, CUT, [cup])
    # Face plateau : tout ce qui est sous z_b disparaît -> une seule face plane, pas de support
    b.extrude(b.rect(cx, cy, 500, 500, 0), z_b - 80, 80, CUT, [cup])
    return cup


def make(design):
    """(Re)construit le support dans le composant racine (un document de pièce n'accepte qu'un composant).
    Tout est rangé dans un groupe de timeline, supprimé puis recréé à chaque lancement."""
    design.designType = adsk.fusion.DesignTypes.ParametricDesignType
    P = read_params(design)
    tl = design.timeline
    for g in [tl.timelineGroups.item(i) for i in range(tl.timelineGroups.count)]:
        if g.name == NAME:
            g.deleteMe(True)
    start = tl.count
    cup = build(design.rootComponent, P)
    tl.timelineGroups.add(start, tl.count - 1).name = NAME
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
        vol = make(design).physicalProperties.volume  # cm3
        ui.messageBox(f"{NAME} créé : {vol:.1f} cm3.\nCotes : Modifier > Changer les paramètres, puis relance le script.")
    except Exception:
        if ui:
            ui.messageBox("Erreur :\n" + traceback.format_exc())
