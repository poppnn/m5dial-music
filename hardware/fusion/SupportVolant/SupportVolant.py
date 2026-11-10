"""Fusion 360 : support M5Dial pour volant GT2i Race 350 mm (tulipage 90 mm).
Source de hardware/support_volant.stl. Fixé par une vis M5 + écrou + rondelle large par branche.

Lancer : Utilitaires > Compléments > Scripts > "+" > dossier SupportVolant > Exécuter.
Au premier lancement, les cotes sont créées en paramètres utilisateur (Modifier > Changer les paramètres).
Pour appliquer de nouvelles cotes : modifie-les là, puis relance le script (il remplace le groupe de timeline).

Une seule pièce, imprimée telle quelle (une seule face posée sur le plateau), supports seulement sous l'anneau rehaussé :
- Plaque fine d'un seul contour convexe entre les deux pattes et le tube, ouverte sous le M5Dial (le collier se met
  par-dessous). Elle affleure le bout bas des coins (PAD_MIN = ARM_T), coupés à la verticale côté moyeu.
- Le M5Dial (rapproché du moyeu, à 3-4 mm des pattes) est rehaussé de DIAL_RISE et se fixe comme sur un panneau :
  anneau plat de 3,5 mm (6 nervures d'écrasement), serré dessous par son collier (Ø 49,8 x 12,6). Sous l'anneau,
  rien dans le rayon du collier. Il est porté par un tube plein percé de 2 fenêtres en arche entre 3 piliers de 60°
  (dans l'axe des bras et côté jante) : doigts pour tenir le collier, USB-C (au dos du M5Dial), câble de batterie.
- Chaque patte se pose sous sa branche, sans paroi, crochet ni téton : la pièce se pose d'un seul geste sur les
  deux branches. Une vis M5 passe
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
    ("DEV_X", 58.5, "mm", "Centre M5Dial, X (au plus près du moyeu : ~3 mm des pattes)"),
    ("DEV_Y", -57.5, "mm", "Centre M5Dial, Y"),
    # Volant OMP Corsica 350, dish 95. Photos de l'utilisateur (règle + entraxe 6 x 70, perspective corrigée) :
    # pente des branches ~43° ; au pied à coulisse : épaisseur 5, trous Ø 16, largeurs 40 (bas) et 38 (droite).
    # Branche droite recalée sur la photo de face OMP (Corsica OD0-1954, Ø 350, dish 95 ; échelle : entraxe 6 x 70) :
    # ses trous sont à la même distance du moyeu que ceux du bas, l'axe des trous monte de 12,7° et la branche
    # s'affine vers la jante (bords à ~7,5° en haut et ~17,4° en bas, soit ~5° par côté).
    ("R_X", 70.2, "mm", "Grand trou de la branche droite, X (centre du moyeu = 0 ; 96 mm entre les trous des deux branches)"),
    ("R_Y", 1.5, "mm", "Grand trou de la branche droite, Y"),
    ("R_RADIAL", 0.0, "deg", "Direction de la branche droite (à 90° de celle du bas, mesuré sur le volant)"),
    ("R_TILT", 40.0, "deg", "Pente de la branche droite (essai sur le volant : 40°)"),
    ("R_DZ", 0.0, "mm", "Profondeur du trou droit par rapport au trou bas (même distance du moyeu : 0)"),
    ("R_SPOKE_W", 38.0, "mm", "Largeur de la branche droite au trou (mesurée)"),
    ("R_TAPER", 4.0, "deg", "Évasement de la branche droite, par côté (plus large vers le moyeu, photo du volant réel ~4°)"),
    ("B_X", 0.0, "mm", "MESURER : grand trou de la branche du bas, X"),
    ("B_Y", -64.0, "mm", "MESURER : grand trou de la branche du bas, Y"),
    ("B_RADIAL", -90.0, "deg", "Direction de la branche du bas"),
    ("B_TILT", 40.0, "deg", "Pente de la branche du bas (essai sur le volant : 40°)"),
    ("B_DZ", 0.0, "mm", "Profondeur du trou bas (référence)"),
    ("B_SPOKE_W", 40.0, "mm", "Largeur de la branche du bas au trou (mesurée)"),
    ("B_TAPER", 4.0, "deg", "Évasement de la branche du bas, par côté (plus large vers le moyeu)"),
    ("SPOKE_T", 5.0, "mm", "Épaisseur de la branche (mesurée : 5 mm)"),
    ("SCREW_D", 5.5, "mm", "Passage de la vis M5 dans la patte"),
    ("NUT_AF", 8.4, "mm", "Empreinte d'écrou M5 : sur plats (écrou 8 + jeu)"),
    ("NUT_FLOOR", 3.0, "mm", "Plastique entre la branche et l'écrou"),
    ("PAD_L", 24.0, "mm", "Patte : longueur le long de la branche"),
    ("PAD_MIN", 0.2, "mm", "Patte : épaisseur au bas de la pente (0,2 = première couche : pièce la plus basse possible)"),
    ("ARM_T", 2.0, "mm", "Épaisseur de la plaque de fond entre les pattes et le tube (6 mm pour la pièce d'essai)"),
    ("FILLET_V", 4.0, "mm", "Congé des arêtes verticales (contour)"),
    ("FILLET_TOP", 1.5, "mm", "Congé des arêtes du dessus des bras"),
    ("BODY_D", 45.0, "mm", "MESURER : Ø du corps du M5Dial derrière la collerette (découpe recommandée Ø45)"),
    ("CRUSH", 0.5, "mm", "Serrage (sur le Ø) des nervures sur le corps du M5Dial"),
    ("RING_T", 3.5, "mm", "Hauteur de l'anneau (le corps du M5Dial et son USB-C ressortent dessous)"),
    ("DIAL_RISE", 27.2, "mm", "Rehausse du M5Dial au-dessus du dessous du support (27,2 : même hauteur sur le volant qu'avec PAD_MIN 6)"),
    ("CLAMP_D", 49.8, "mm", "Ø extérieur du collier de serrage du M5Dial (STL Dial_v1.1 : 49,8, 12,6 de haut)"),
    ("CLAMP_GAP", 1.5, "mm", "Jeu radial autour du collier (il doit tourner librement sous l'anneau)"),
    ("TUBE_WALL", 3.5, "mm", "Épaisseur du tube qui porte l'anneau rehaussé (et la gorge de la baïonnette de la batterie)"),
    ("LEG_ANG", 36.0, "deg", "Largeur angulaire des 3 piliers du tube (entre eux : grandes fenêtres pour serrer le collier)"),
    ("BAT_L", 54.0, "mm", "Coque batterie : longueur intérieure (LiPo 103450 = 50 + carte de protection et fils)"),
    ("BAT_W", 36.0, "mm", "Coque batterie : largeur intérieure (LiPo 103450 = 34)"),
    ("BAT_T", 11.0, "mm", "Coque batterie : profondeur intérieure (LiPo 103450 = 10)"),
    ("SHELL_WALL", 1.6, "mm", "Coque batterie : épaisseur des parois et du fond"),
    ("BAT_ROT", 0.0, "deg", "Coque batterie : orientation sous le M5Dial (0 = grand côté dans l'axe moyeu -> M5Dial)"),
    ("BAT_TWIST", 35.0, "deg", "Coque batterie : quart de tour de la baïonnette (vissage vu de dessous)"),
    ("TEST_FIT", 0, "", "1 = pièce d'essai rapide : les deux pattes reliées par la plaque, sans tube ni anneau du M5Dial"),
]
OBSOLETE = ["WALL", "FLOOR", "CUP_H", "NOTCH_W", "NOTCH_ANGLE", "RIBS", "RIB", "PAD_T",  # ancienne coupelle
            "SEAT_T", "SEAT_D", "SLOT_W", "SLOT_L",  # ancienne fixation par vis M5
            "FLOOR_HOLE_D", "BODY_H", "PORT_W", "PORT_ANG", "SPOKE_W", "PAD_W",  # ancien boîtier arrière, encoche USB-C
            "CASE_WALL", "BAT_X", "BAT_Y",  # ancien logement / plateau batterie
            "ARM_W", "ARM_ROOT_W",  # anciens bras (remplacés par une plaque d'un seul contour)
            "PEG_D", "CLIP_LIP", "CLIP_LIP_T", "WIN_ANG", "CLIP_WALL", "CLIP_GAP"]  # anciens téton, crochets, parois

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

    def rotate(self, body, axis, pt, ang, dz=0, dx=0, dy=0):
        """Rotation de `ang` degrés autour de l'axe (direction axis, passant par pt en mm), puis décalage (mm)."""
        m = adsk.core.Matrix3D.create()
        m.setToRotation(math.radians(ang), adsk.core.Vector3D.create(*axis),
                        adsk.core.Point3D.create(pt[0] / 10, pt[1] / 10, pt[2] / 10))
        tr = m.translation
        tr.x += dx / 10
        tr.y += dy / 10
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
        body = edges[0].body
        vol = body.physicalProperties.volume
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
                feat = self.f.filletFeatures.add(inp)
                # Fusion peut "réussir" un congé qui vide le corps : on l'annule et on essaie plus petit
                if feat.bodies.count and feat.bodies.item(0).physicalProperties.volume < 0.9 * vol:
                    feat.deleteMe()
                    continue
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
    """Largeur de la patte : celle de la branche au bout côté jante (elle s'élargit vers le moyeu), moins 1 mm :
    la patte ne dépasse pas des bords de la branche."""
    return P[k + "_SPOKE_W"] - P["PAD_L"] * math.tan(math.radians(P[k + "_TAPER"])) - 1


def band_w(P, k):
    """Largeur dégagée au-dessus de la face arrière de la branche, sur 35 mm de part et d'autre du trou."""
    return P[k + "_SPOKE_W"] + 70 * math.tan(math.radians(P[k + "_TAPER"])) + 2


def hull(pts):
    """Enveloppe convexe (chaîne monotone), sens trigonométrique."""
    pts = sorted(set((round(x, 4), round(y, 4)) for x, y in pts))

    def half(seq):
        out = []
        for p in seq:
            while len(out) >= 2 and ((out[-1][0] - out[-2][0]) * (p[1] - out[-2][1])
                                     - (out[-1][1] - out[-2][1]) * (p[0] - out[-2][0])) <= 0:
                out.pop()
            out.append(p)
        return out[:-1]
    return half(pts) + half(pts[::-1])


assert hull([(0, 0), (2, 0), (1, 1), (2, 2), (0, 2)]) == [(0, 0), (2, 0), (2, 2), (0, 2)]


def steep(e):
    """Arête droite plutôt verticale (contour vu de dessus)."""
    g = e.geometry
    if not isinstance(g, adsk.core.Line3D):
        return False
    a, c = g.startPoint, g.endPoint
    L = a.distanceTo(c)
    return L > 0 and abs(c.z - a.z) / L > 0.5


def sector(cx, cy, r0, r1, a0, a1):
    """Secteur d'anneau (angles en degrés, a0 < a1) en polygone, pas de 3° au plus."""
    n = max(2, int(math.ceil((a1 - a0) / 3)))
    arc = [a0 + (a1 - a0) * i / n for i in range(n + 1)]
    pol = lambda r, a: (cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a)))
    return [pol(r1, a) for a in arc] + [pol(r0, a) for a in reversed(arc)]


# Baïonnette de la coque batterie (z depuis le dessous du support, z_b) : 3 tenons sur la couronne de la coque
# passent par 3 encoches verticales, puis tournent de BAT_TWIST dans une gorge ; le dessous de la gorge les retient.
BOSS_H, BOSS_T = 5.0, 3.0    # couronne pleine de la coque, dans l'alésage du collier (jeu radial 0,3)
LUGS = (60, 180, 300)        # tenons, en degrés depuis le bout ouvert de la coque (0 : fente du câble)
LUG_ANG, LUG_P = 30.0, 1.5   # tenons : largeur angulaire, saillie hors de la couronne
LUG_Z0, LUG_Z1 = 2.6, 5.0    # tenons : hauteur (2,4 mm)
GROOVE_Z0, GROOVE_Z1, GROOVE_D = 2.4, 5.2, 1.6  # gorge dans le pied du tube : rebord de 2,4 mm sous les tenons
BUMP_D, BUMP_H = 2.0, 0.4    # bossage sous chaque tenon : frotte 0,2 mm sur la gorge puis tombe dans un creux (clic)
WIRE_W = 8.0                 # fente du câble de la batterie dans le dessus de la coque, depuis le bout ouvert


def battery(b, P, cup, cx, cy, z_b, r_clr, a_out):
    """Coque de la LiPo sous le tube du M5Dial (corps séparé "Coque batterie", imprimé fond sur le plateau) et,
    dans le pied du tube : encoches d'entrée, gorge et creux de verrouillage. La batterie se glisse par le bout
    ouvert de la coque (côté jante), son câble remonte par la fente jusqu'au M5Dial."""
    ang = a_out + P["BAT_ROT"]   # grand côté de la coque, bout ouvert vers l'extérieur du volant
    u = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
    at = lambda d: (cx + d * u[0], cy + d * u[1])  # point sur le grand axe, à d mm du centre
    r_b = r_clr - 0.3            # couronne
    r_g = r_clr + GROOVE_D       # fond de la gorge
    half = LUG_ANG / 2 + 2       # encoche et gorge : 2° de jeu de chaque côté du tenon
    rm = r_b + LUG_P / 2
    bump = lambda a: (cx + rm * math.cos(math.radians(a)), cy + rm * math.sin(math.radians(a)))
    for k in LUGS:
        af = ang + k
        ae = af - P["BAT_TWIST"]
        b.extrude(b.poly(sector(cx, cy, r_clr - 0.5, r_g, ae - half, ae + half)), z_b - 1, 1 + GROOVE_Z1, CUT, [cup])
        b.extrude(b.poly(sector(cx, cy, r_clr - 0.5, r_g, ae - half, af + half)), z_b + GROOVE_Z0,
                  GROOVE_Z1 - GROOVE_Z0, CUT, [cup])
        b.extrude(b.circles([bump(af)], BUMP_D / 2 + 0.4), z_b + GROOVE_Z0 - 0.6, 0.7, CUT, [cup])

    wall, L, W, T = P["SHELL_WALL"], P["BAT_L"], P["BAT_W"], P["BAT_T"]
    h_tray = T + 2 * wall        # fond, logement, dessus plein
    shell = b.solid(b.rect(cx, cy, L + 2 * wall, W + 2 * wall, ang), z_b - h_tray, h_tray)
    shell.name = "Coque batterie"
    # Collerette à 45° du bord de la coque jusqu'à la couronne : imprimable sans support
    w2 = W / 2 + wall
    if r_b > w2:
        ext = b.f.extrudeFeatures
        inp = ext.createInput(b.circles([(cx, cy)], w2).profiles.item(0), NEW)
        inp.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(adsk.core.ValueInput.createByReal((r_b - w2) / 10)),
                             adsk.fusion.ExtentDirections.PositiveExtentDirection, adsk.core.ValueInput.createByString("45 deg"))
        inp.startExtent = adsk.fusion.OffsetStartDefinition.create(adsk.core.ValueInput.createByReal((z_b - (r_b - w2)) / 10))
        b.combine(shell, ext.add(inp).bodies.item(0), JOIN)
    b.combine(shell, b.solid(b.circles([(cx, cy)], r_b), z_b, BOSS_H), JOIN)  # couronne
    for k in LUGS:
        af = ang + k
        b.combine(shell, b.solid(b.poly(sector(cx, cy, r_b - 0.2, r_b + LUG_P, af - LUG_ANG / 2, af + LUG_ANG / 2)),
                                 z_b + LUG_Z0, LUG_Z1 - LUG_Z0), JOIN)
        b.combine(shell, b.solid(b.circles([bump(af)], BUMP_D / 2), z_b + LUG_Z0 - BUMP_H, BUMP_H), JOIN)
    # Logement ouvert au bout côté jante ; couronne évidée ; fente du câble du bout ouvert jusque dans la couronne
    b.extrude(b.rect(*at((wall + 1) / 2), L + wall + 1, W, ang), z_b - wall - T, T, CUT, [shell])
    b.extrude(b.circles([(cx, cy)], r_b - BOSS_T), z_b, BOSS_H + 1, CUT, [shell])
    x0, x1 = r_b - BOSS_T - 6, L / 2 + wall + 1
    b.extrude(b.rect(*at((x0 + x1) / 2), x1 - x0, WIRE_W, ang), z_b - wall - 0.5, wall + BOSS_H + 2, CUT, [shell])
    # Petit rebord de 0,8 mm au fond, à l'entrée : la batterie ne ressort pas toute seule
    b.combine(shell, b.solid(b.rect(*at(L / 2 - 0.75), 1.5, W, ang), z_b - wall - T, 0.8), JOIN)
    return shell


def build(comp, P):
    b = Builder(comp)
    cx, cy = P["DEV_X"], P["DEV_Y"]
    r_out = P["DIAL_D"] / 2
    # Plan du plateau (= dos de la plaque, où se colle la batterie) : sous le bout le plus bas des coins
    z_b = min(P[k + "_DZ"] - P["PAD_L"] / 2 * math.sin(math.radians(P[k + "_TILT"])) for k in ("R", "B")) - P["PAD_MIN"]
    test = P["TEST_FIT"] >= 0.5  # essai d'angle : mêmes pattes et même plaque entre elles, sans la partie M5Dial
    slab = max(P["ARM_T"], 6) if test else P["ARM_T"]  # pièce d'essai : seule la plaque relie les pattes
    z_ring = z_b + P["DIAL_RISE"]  # bas de l'anneau qui tient le M5Dial
    raised = P["DIAL_RISE"] > 9
    r_clr = P["CLAMP_D"] / 2 + P["CLAMP_GAP"]  # vide sous l'anneau rehaussé : le collier s'y visse
    r_base = r_clr + P["TUBE_WALL"] if raised else r_out
    z_win = z_b + GROOVE_Z1 + 1.2  # bas des fenêtres du tube, au-dessus de la gorge de la baïonnette

    # Plaque d'un seul contour convexe : base du tube + côté intérieur des deux pattes (pas de bras ni de recoins)
    rp = r_base - 1  # un peu dans le tube : le contour ne frôle pas le cylindre (échec "osculating curves")
    pts = [] if test else [(cx + rp * math.cos(math.radians(a)), cy + rp * math.sin(math.radians(a))) for a in range(0, 360, 2)]
    for k in ("R", "B"):
        x, y, rad, til = P[k + "_X"], P[k + "_Y"], P[k + "_RADIAL"], P[k + "_TILT"]
        u = (math.cos(math.radians(rad)), math.sin(math.radians(rad)))
        n = (-u[1], u[0])
        sd = 1 if n[0] * (cx - x) + n[1] * (cy - y) > 0 else -1  # côté du M5Dial
        # Bout côté moyeu : juste hors de la bande dégagée au-dessus de la branche (pad_w + 4), sinon la plaque y est
        # rognée en rainures ; bout côté jante : sous la patte, pour que plaque et patte ne fassent qu'un.
        hl = P["PAD_L"] / 2 * math.cos(math.radians(til))
        for su, hw in ((-1, pad_w(P, k) / 2 + 2.5), (1, pad_w(P, k) / 2 - P["FILLET_V"])):
            pts.append((x + su * hl * u[0] + sd * hw * n[0], y + su * hl * u[1] + sd * hw * n[1]))
    cup = b.solid(b.poly(hull(pts)), z_b, slab)
    cup.name = NAME

    a_out = math.degrees(math.atan2(cy, cx))  # direction opposée au moyeu
    for k in ("R", "B"):
        x, y, rad, til, dz = (P[k + "_X"], P[k + "_Y"], P[k + "_RADIAL"], P[k + "_TILT"], P[k + "_DZ"])

        def tilted(sk, z0, h):
            body = b.solid(sk, z0, h)
            b.tilt(body, x, y, rad, til, dz)
            return body

        # Coin sous la branche : dessus = face arrière de la branche. Côté M5Dial, il est prolongé (même pente)
        # jusque dans la paroi du tube : patte et tube ne font qu'un. Coin allongé côté moyeu puis recoupé à la
        # verticale : bout net, sans face penchée en contre-dépouille.
        ux, uy = math.cos(math.radians(rad)), math.sin(math.radians(rad))
        nx, ny = -uy, ux
        sd = 1 if nx * (cx - x) + ny * (cy - y) > 0 else -1  # côté du M5Dial
        pw = pad_w(P, k)
        # Largeur ajoutée côté M5Dial : jusqu'à 3 mm dans la paroi du tube au droit du bout côté jante de la patte
        # (le plus loin du tube) ; ce qui dépasse dans l'alésage est retiré avec lui.
        t_end = P["PAD_L"] / 2 * math.cos(math.radians(til)) - (ux * (cx - x) + uy * (cy - y))
        reach = abs(nx * (cx - x) + ny * (cy - y)) - math.sqrt(max(0.0, r_base ** 2 - t_end ** 2)) + 3
        join = 0 if test else max(0, reach - pw / 2)
        ox, oy = sd * join / 2 * nx, sd * join / 2 * ny
        b.combine(cup, tilted(b.rect(x - 10 * ux + ox, y - 10 * uy + oy, P["PAD_L"] + 20, pw + join, rad), -40, 40), JOIN)
        hl = P["PAD_L"] / 2 * math.cos(math.radians(til)) + 15
        b.combine(cup, b.solid(b.rect(x - hl * ux + ox, y - hl * uy + oy, 30, pw + join + 4, rad), z_b - 1, 100), CUT)
        # Rien au-dessus de la face arrière de la branche (elle s'élargit vers le moyeu)
        b.combine(cup, tilted(b.rect(x, y, 70, band_w(P, k), rad), 0, 40), CUT)
    if not test:  # tube ajouté après les recoupes des pattes, qui ne doivent pas l'entailler
        b.combine(cup, b.solid(b.circles([(cx, cy)], r_base), z_b, max(slab, P["DIAL_RISE"] + P["RING_T"])), JOIN)

    # Passage du corps du M5Dial (les nervures le serrent, plus bas)
    r_in = P["BODY_D"] / 2 + 0.3
    z_floor = z_b - 1  # ouvert dessous : le collier se met par-dessous
    if not test:
        b.extrude(b.circles([(cx, cy)], r_in), z_floor, 200, CUT, [cup])
    if raised and not test:  # sous l'anneau plat de RING_T : rien dans le rayon du collier
        b.extrude(b.circles([(cx, cy)], r_clr), z_floor, z_ring - z_floor, CUT, [cup])
    # Anneau de RING_T seulement : le corps du M5Dial (et son USB-C) ressort dessous. Sans rehausse, la plaque autour
    # est abaissée au même niveau, 1 mm plus large que la collerette pour qu'elle ne frotte pas.
    if P["DIAL_RISE"] + P["RING_T"] < slab and not test:
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

    # Tube plein, percé de fenêtres en arche entre 3 piliers (dans l'axe des deux bras et à l'opposé du moyeu) :
    # les doigts y passent pour tenir le collier, ainsi que la prise USB-C (au dos du M5Dial) et le câble de batterie.
    # Le haut en arche s'imprime sans support et garde le tube rigide.
    if raised and not test:
        legs = sorted(math.degrees(math.atan2(P[k + "_Y"] - cy, P[k + "_X"] - cx)) % 360 for k in ("R", "B"))
        legs = sorted(legs + [a_out % 360])
        h = z_ring - 1 - z_win  # hauteur de fenêtre, 1 mm sous l'anneau
        for a1, a2 in zip(legs, legs[1:] + [legs[0] + 360]):
            span = a2 - a1 - P["LEG_ANG"]
            ww = min(2 * r_clr * math.sin(math.radians(span / 2)), 2 * (h - 3))  # largeur à l'intérieur du tube
            if ww < 12:
                continue
            ra = ww / 2
            pts = ([(ra, 0), (ra, h - ra)] + [(ra * math.cos(math.radians(t)), h - ra + ra * math.sin(math.radians(t)))
                                               for t in range(15, 180, 15)] + [(-ra, h - ra), (-ra, 0)])
            win = b.solid(b.poly(pts), 0, r_base + 0.5)  # arche dans le plan XZ, poussée selon -Y : la paroi seule
            b.rotate(win, (1, 0, 0), (0, 0, 0), 90)
            b.rotate(win, (0, 0, 1), (0, 0, 0), (a1 + a2) / 2 + 90, z_win, cx, cy)
            b.combine(cup, win, CUT)

    # 6 nervures d'écrasement dans l'anneau : serrent le corps du M5Dial, absorbent la tolérance d'impression
    tip, root = P["BODY_D"] / 2 - P["CRUSH"] / 2, r_in + 0.6
    for i in range(0 if test else 6):
        a = math.radians(30 + 60 * i)
        rc = (tip + root) / 2
        rib = b.solid(b.rect(cx + rc * math.cos(a), cy + rc * math.sin(a), root - tip, 1.2, math.degrees(a)), z_ring, P["RING_T"])
        b.combine(cup, rib, JOIN)

    # Vis et écrou, construits dans le repère de la branche puis inclinés : z = 0 face arrière de la branche.
    # Pas de parois de guidage (retirées après essai) : la vis seule positionne la patte.
    for k in ("R", "B"):
        x, y, rad, til, dz = (P[k + "_X"], P[k + "_Y"], P[k + "_RADIAL"], P[k + "_TILT"], P[k + "_DZ"])
        # Vis M5 par le trou de la branche (rondelle large côté conducteur), écrou dans une empreinte au dos
        hole = b.solid(b.circles([(x, y)], P["SCREW_D"] / 2), -40, 41)
        rh = P["NUT_AF"] / math.sqrt(3)
        nut = b.solid(b.poly([(x + rh * math.cos(math.radians(rad + 60 * j)), y + rh * math.sin(math.radians(rad + 60 * j)))
                              for j in range(6)]), -40, 40 - P["NUT_FLOOR"])
        for t in (hole, nut):
            b.tilt(t, x, y, rad, til, dz)
            b.combine(cup, t, CUT)
    if raised and not test:
        battery(b, P, cup, cx, cy, z_b, r_clr, a_out)
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
