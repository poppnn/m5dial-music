"""Fusion 360 : support M5Dial pour volant GT2i Race 350 mm (tulipage 90 mm).
Source de hardware/support_volant.stl. Fixé par une vis M5 + écrou + rondelle large par branche ; batterie collée.

Lancer : Utilitaires > Compléments > Scripts > "+" > dossier SupportVolant > Exécuter.
Au premier lancement, les cotes sont créées en paramètres utilisateur (Modifier > Changer les paramètres).
Pour appliquer de nouvelles cotes : modifie-les là, puis relance le script (il remplace le groupe de timeline).

Une seule pièce, imprimée telle quelle (une seule face posée sur le plateau), sans support :
- Plaque fine et pleine entre les deux branches (anneau, bras et zone entre eux). La LiPo 103450 se colle au dos,
  centrée vers (52, -42), grand côté incliné de ~30° (86 % d'appui) ; en vraie diagonale (~48°), centrée vers
  (42, -53) (80 % d'appui). Là, elle ne touche ni les branches (plus en arrière près du moyeu) ni le corps du M5Dial.
- Le M5Dial est rehaussé (DIAL_RISE) pour se rapprocher de la jante : son corps s'enfonce dans un anneau de 3,5 mm
  serré par 6 nervures d'écrasement, porté par 3 pieds (tube ajouré) ; la collerette repose dessus et le corps
  pend dans le tube : l'USB-C et le câble de batterie passent par une des 3 fenêtres.
- Chaque patte se pose sur sa branche entre 2 parois de guidage (inclinées comme les bords, la branche s'élargit
  vers le moyeu), sans crochet ni téton : la pièce se pose d'un seul geste sur les deux branches. Une vis M5 passe
  par le grand trou (Ø 16, rondelle large Ø 20+ côté conducteur) et la patte ; l'écrou se loge dans une empreinte
  hexagonale au dos.

Repère : X droite, Y haut (vue de face), Z vers le conducteur, origine = centre du moyeu,
Z = 0 = face arrière des branches au niveau des trous.
"""
import math
import traceback

import adsk.core
import adsk.fusion

NAME = "Support M5Dial"

# (nom, valeur par défaut, unité, commentaire). Les cotes MESURER viennent d'une photo ou d'une estimation.
PARAMS = [
    ("DIAL_D", 51.0, "mm", "Ø extérieur de l'anneau = Ø du M5Dial"),
    ("DEV_X", 78.6, "mm", "MESURER : centre M5Dial, X"),
    ("DEV_Y", -73.8, "mm", "MESURER : centre M5Dial, Y"),
    # Mesures sur photos (règle + entraxe 6 x 70 du moyeu, perspective corrigée) d'un OMP 350 tulipage 90 :
    # branches latérales relevées de ~18°, pente des branches ~43°, épaisseur 5, trous Ø 16 (mesurés).
    ("R_X", 66.7, "mm", "MESURER : grand trou de la branche droite, X (centre du moyeu = 0)"),
    ("R_Y", 6.0, "mm", "MESURER : grand trou de la branche droite, Y"),
    ("R_RADIAL", 18.0, "deg", "MESURER : direction de la branche droite (relevée de ~18° sur l'OMP)"),
    ("R_TILT", 43.0, "deg", "MESURER : pente de la branche droite (photo de profil : ~43°)"),
    ("R_DZ", 2.8, "mm", "Profondeur du trou droit par rapport au trou bas (il est plus loin du moyeu)"),
    ("R_SPOKE_W", 38.0, "mm", "Largeur de la branche droite au trou (mesurée)"),
    ("R_TAPER", 1.0, "deg", "Évasement de la branche droite, par côté (plus large vers le moyeu)"),
    ("B_X", 0.0, "mm", "MESURER : grand trou de la branche du bas, X"),
    ("B_Y", -64.0, "mm", "MESURER : grand trou de la branche du bas, Y"),
    ("B_RADIAL", -90.0, "deg", "Direction de la branche du bas"),
    ("B_TILT", 43.0, "deg", "MESURER : pente de la branche du bas (photo de profil : ~43°)"),
    ("B_DZ", 0.0, "mm", "Profondeur du trou bas (référence)"),
    ("B_SPOKE_W", 40.0, "mm", "Largeur de la branche du bas au trou (mesurée)"),
    ("B_TAPER", 4.0, "deg", "Évasement de la branche du bas, par côté (plus large vers le moyeu)"),
    ("SPOKE_T", 5.0, "mm", "Épaisseur de la branche (mesurée : 5 mm)"),
    ("SCREW_D", 5.5, "mm", "Passage de la vis M5 dans la patte"),
    ("NUT_AF", 8.4, "mm", "Empreinte d'écrou M5 : sur plats (écrou 8 + jeu)"),
    ("NUT_FLOOR", 3.0, "mm", "Plastique entre la branche et l'écrou"),
    ("PAD_L", 24.0, "mm", "Patte : longueur le long de la branche"),
    ("PAD_MIN", 3.0, "mm", "Patte : épaisseur mini (bout bas du coin)"),
    ("CLIP_WALL", 2.2, "mm", "Parois de guidage : épaisseur"),
    ("CLIP_GAP", 0.3, "mm", "Parois de guidage : jeu avec le bord de la branche"),
    ("ARM_W", 18.0, "mm", "Bras : largeur côté patte"),
    ("ARM_ROOT_W", 30.0, "mm", "Bras : largeur côté anneau (effilé)"),
    ("ARM_T", 6.0, "mm", "Hauteur du dessus de la plaque au-dessus du bas des pattes (assise du M5Dial)"),
    ("FILLET_V", 4.0, "mm", "Congé des arêtes verticales (contour)"),
    ("FILLET_TOP", 1.5, "mm", "Congé des arêtes du dessus des bras"),
    ("BODY_D", 45.0, "mm", "MESURER : Ø du corps du M5Dial derrière la collerette (découpe recommandée Ø45)"),
    ("CRUSH", 0.5, "mm", "Serrage (sur le Ø) des nervures sur le corps du M5Dial"),
    ("RING_T", 3.5, "mm", "Hauteur de l'anneau (le corps du M5Dial et son USB-C ressortent dessous)"),
    ("DIAL_RISE", 30.0, "mm", "Rehausse du M5Dial au-dessus de la plaque (0 = anneau posé sur la plaque)"),
    ("WIN_ANG", 40.0, "deg", "Largeur angulaire des 3 fenêtres du tube (USB-C, câble)"),
]
OBSOLETE = ["WALL", "FLOOR", "CUP_H", "NOTCH_W", "NOTCH_ANGLE", "RIBS", "RIB", "PAD_T",  # ancienne coupelle
            "SEAT_T", "SEAT_D", "SLOT_W", "SLOT_L",  # ancienne fixation par vis M5
            "FLOOR_HOLE_D", "BODY_H", "PORT_W", "PORT_ANG", "SPOKE_W", "PAD_W",  # ancien boîtier arrière, encoche USB-C
            "CASE_WALL", "BAT_L", "BAT_W", "BAT_T", "BAT_X", "BAT_Y",  # ancien logement batterie
            "PEG_D", "CLIP_LIP", "CLIP_LIP_T"]  # anciens téton et crochets (pièce impossible à poser sur 2 branches)

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
        self.rotate(body, (-math.sin(r), math.cos(r), 0), (x, y, 0), -tilt, dz)

    def rotate(self, body, axis, pt, ang, dz=0):
        """Rotation de `ang` degrés autour de l'axe (direction axis, passant par pt en mm), puis décalage en Z."""
        m = adsk.core.Matrix3D.create()
        m.setToRotation(math.radians(ang), adsk.core.Vector3D.create(*axis),
                        adsk.core.Point3D.create(pt[0] / 10, pt[1] / 10, pt[2] / 10))
        tr = m.translation
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

    def fillet(self, edges, r, chain=True):
        """Congé constant ; si Fusion refuse ce rayon, essaie plus petit. Renvoie le rayon appliqué (0 = aucun)."""
        if not edges:
            return 0
        for rr in (r, r * 0.6, r * 0.35):
            coll = adsk.core.ObjectCollection.create()
            for e in edges:
                coll.add(e)
            try:
                inp = self.f.filletFeatures.createInput()
                rad = adsk.core.ValueInput.createByReal(rr / 10)
                if hasattr(inp, "edgeSetInputs"):
                    inp.edgeSetInputs.addConstantRadiusEdgeSet(coll, rad, chain)
                else:
                    inp.addConstantRadiusEdgeSet(coll, rad, chain)
                self.f.filletFeatures.add(inp)
                return rr
            except RuntimeError:
                pass
        return 0

    def combine(self, target, tool, op):
        coll = adsk.core.ObjectCollection.create()
        coll.add(tool)
        inp = self.f.combineFeatures.createInput(target, coll)
        inp.operation = op
        inp.isKeepToolBodies = False
        self.f.combineFeatures.add(inp)


def pad_w(P, k):
    """Largeur de la patte : branche + jeu et parois des deux côtés + évasement sur toute la longueur."""
    return (P[k + "_SPOKE_W"] + 2 * (P["CLIP_GAP"] + P["CLIP_WALL"])
            + P["PAD_L"] * math.tan(math.radians(P[k + "_TAPER"])) + 1)


def steep(e):
    """Arête droite plutôt verticale (contour vu de dessus)."""
    g = e.geometry
    if not isinstance(g, adsk.core.Line3D):
        return False
    a, c = g.startPoint, g.endPoint
    L = a.distanceTo(c)
    return L > 0 and abs(c.z - a.z) / L > 0.5


def build(comp, P):
    b = Builder(comp)
    cx, cy = P["DEV_X"], P["DEV_Y"]
    r_out = P["DIAL_D"] / 2
    # Plan du plateau (= dos de la plaque, où se colle la batterie) : sous le bout le plus bas des coins
    z_b = min(P[k + "_DZ"] - P["PAD_L"] / 2 * math.sin(math.radians(P[k + "_TILT"])) for k in ("R", "B")) - P["PAD_MIN"]
    slab = P["ARM_T"]
    z_ring = z_b + P["DIAL_RISE"]  # bas de l'anneau qui tient le M5Dial

    cup = b.solid(b.circles([(cx, cy)], r_out), z_b, max(slab, P["DIAL_RISE"] + P["RING_T"]))  # tube jusqu'à l'anneau
    cup.name = NAME
    # Zone entre les bras remplie (triangle M5Dial / trou droit / trou bas), rognée plus loin le long des branches
    b.combine(cup, b.solid(b.poly([(cx, cy), (P["R_X"], P["R_Y"]), (P["B_X"], P["B_Y"])]), z_b, slab), JOIN)

    for k in ("R", "B"):
        x, y, rad, til, dz = (P[k + "_X"], P[k + "_Y"], P[k + "_RADIAL"], P[k + "_TILT"], P[k + "_DZ"])

        def tilted(sk, z0, h):
            body = b.solid(sk, z0, h)
            b.tilt(body, x, y, rad, til, dz)
            return body

        # Bras posé sur le plateau, de l'anneau jusqu'au trou, puis coin : dessus = face arrière de la branche
        ang = math.degrees(math.atan2(y - cy, x - cx))
        u = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
        n, hw, hw0 = (-u[1], u[0]), P["ARM_W"] / 2, P["ARM_ROOT_W"] / 2
        root = (cx + u[0] * (r_out - 6), cy + u[1] * (r_out - 6))
        arm = b.solid(b.poly([(root[0] + n[0] * hw0, root[1] + n[1] * hw0), (x + n[0] * hw, y + n[1] * hw),
                              (x - n[0] * hw, y - n[1] * hw), (root[0] - n[0] * hw0, root[1] - n[1] * hw0)]),
                      z_b, slab)
        b.combine(cup, arm, JOIN)
        b.combine(cup, tilted(b.rect(x, y, P["PAD_L"], pad_w(P, k), rad), -40, 40), JOIN)  # rogné au plateau plus bas
        # Rien au-dessus de la face arrière de la branche, ni contre les parois des clips (2 mm de jeu)
        b.combine(cup, tilted(b.rect(x, y, 70, pad_w(P, k) + 4, rad), 0, 40), CUT)

    # Passage du corps du M5Dial (les nervures le serrent, plus bas)
    r_in = P["BODY_D"] / 2 + 0.3
    b.extrude(b.circles([(cx, cy)], r_in), z_b - 1, 200, CUT, [cup])
    # Anneau de RING_T seulement : le corps du M5Dial (et son USB-C) ressort dessous. Sans rehausse, la plaque autour
    # est abaissée au même niveau, 1 mm plus large que la collerette pour qu'elle ne frotte pas.
    if P["DIAL_RISE"] + P["RING_T"] < slab:
        b.extrude(b.circles([(cx, cy)], r_out + 1), z_b + P["RING_T"], 60, CUT, [cup])
    # Face plateau : tout ce qui est sous z_b disparaît -> une seule face plane, pas de support
    b.extrude(b.rect(cx, cy, 500, 500, 0), z_b - 80, 80, CUT, [cup])

    # Esthétique : arêtes (quasi) verticales du contour arrondies (avant les clips, trop fins pour un congé)
    # Une arête à la fois : un seul coin impossible (arête trop courte) ne doit pas annuler tous les autres
    mids = [(e.geometry.startPoint.x + e.geometry.endPoint.x, e.geometry.startPoint.y + e.geometry.endPoint.y)
            for e in cup.edges if steep(e)]
    for mx, my in mids:
        cur = [e for e in cup.edges if steep(e) and abs(e.geometry.startPoint.x + e.geometry.endPoint.x - mx) < 1e-3
               and abs(e.geometry.startPoint.y + e.geometry.endPoint.y - my) < 1e-3]
        b.fillet(cur[:1], P["FILLET_V"])
    # Dessus des bras adouci ; pas les cercles de l'anneau (assise du M5Dial) ni la face plateau
    z_top = (z_b + slab) / 10
    tops = [f for f in cup.faces if isinstance(f.geometry, adsk.core.Plane)
            and abs(abs(f.geometry.normal.z) - 1) < 1e-6 and abs(f.pointOnFace.z - z_top) < 1e-3]

    def ring_edge(e):
        g = e.geometry
        return isinstance(g, (adsk.core.Circle3D, adsk.core.Arc3D)) and math.hypot(g.center.x - cx / 10, g.center.y - cy / 10) < 1e-3

    b.fillet([e for e in {e.tempId: e for f in tops for e in f.edges}.values() if not ring_edge(e)], P["FILLET_TOP"], False)

    # 3 fenêtres dans le tube, entre la plaque et l'anneau : USB-C et câble de batterie. Une face au plus loin du moyeu.
    if P["DIAL_RISE"] > slab + 3:
        a0, w, rw = math.degrees(math.atan2(cy, cx)), P["WIN_ANG"] / 2, r_out + 3
        for i in range(3):
            a = a0 + 120 * i
            pts = [(cx, cy)] + [(cx + rw * math.cos(math.radians(a + d)), cy + rw * math.sin(math.radians(a + d)))
                                for d in (-w, 0, w)]
            b.extrude(b.poly(pts), z_b + slab, P["DIAL_RISE"] - slab, CUT, [cup])

    # 6 nervures d'écrasement dans l'anneau : serrent le corps du M5Dial, absorbent la tolérance d'impression
    tip, root = P["BODY_D"] / 2 - P["CRUSH"] / 2, r_in + 0.6
    for i in range(6):
        a = math.radians(30 + 60 * i)
        rc = (tip + root) / 2
        rib = b.solid(b.rect(cx + rc * math.cos(a), cy + rc * math.sin(a), root - tip, 1.2, math.degrees(a)), z_ring, P["RING_T"])
        b.combine(cup, rib, JOIN)

    # Parois de guidage et vis, construites dans le repère de la branche puis inclinées : z = 0 face arrière,
    # z = SPOKE_T face avant. Pas de crochet : la pièce se pose sur les deux branches d'un même geste (selon Z).
    clip_l = P["PAD_L"] - 2 * P["FILLET_V"]
    for k in ("R", "B"):
        x, y, rad, til, dz = (P[k + "_X"], P[k + "_Y"], P[k + "_RADIAL"], P[k + "_TILT"], P[k + "_DZ"])
        ur = (math.cos(math.radians(rad)), math.sin(math.radians(rad)))
        nr = (-ur[1], ur[0])
        e = P[k + "_SPOKE_W"] / 2 + P["CLIP_GAP"]  # face intérieure des parois, au centre de la patte
        tp = P[k + "_TAPER"]  # parois inclinées comme les bords de la branche (plus large vers le moyeu)
        for sd in (1, -1):
            def o(dist):
                return x + nr[0] * sd * dist, y + nr[1] * sd * dist

            wa = rad - sd * tp  # direction de la paroi : suit le bord de la branche
            wall = b.solid(b.rect(*o(e + P["CLIP_WALL"] / 2), clip_l, P["CLIP_WALL"], wa), -1, P["SPOKE_T"] + 1)
            b.tilt(wall, x, y, rad, til, dz)
            b.combine(cup, wall, JOIN)
        # Vis M5 par le trou de la branche (rondelle large côté conducteur), écrou dans une empreinte au dos
        hole = b.solid(b.circles([(x, y)], P["SCREW_D"] / 2), -40, 41)
        rh = P["NUT_AF"] / math.sqrt(3)
        nut = b.solid(b.poly([(x + rh * math.cos(math.radians(rad + 60 * j)), y + rh * math.sin(math.radians(rad + 60 * j)))
                              for j in range(6)]), -40, 40 - P["NUT_FLOOR"])
        for t in (hole, nut):
            b.tilt(t, x, y, rad, til, dz)
            b.combine(cup, t, CUT)
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
