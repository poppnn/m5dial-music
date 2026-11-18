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
    ("ARM_T", 6.0, "mm", "Pièce d'essai : épaisseur de la plaque qui relie les deux pattes"),
    ("FILLET_V", 4.0, "mm", "Congé des arêtes verticales (contour)"),
    ("FILLET_TOP", 1.5, "mm", "Congé des arêtes du dessus des bras"),
    ("BODY_D", 45.0, "mm", "MESURER : Ø du corps du M5Dial derrière la collerette (découpe recommandée Ø45)"),
    ("CRUSH", 0.5, "mm", "Serrage (sur le Ø) des nervures sur le corps du M5Dial"),
    ("RING_T", 3.5, "mm", "Hauteur de l'anneau (le corps du M5Dial et son USB-C ressortent dessous)"),
    ("DIAL_RISE", 27.2, "mm", "Rehausse du M5Dial au-dessus du dessous du support (27,2 : même hauteur sur le volant qu'avec PAD_MIN 6)"),
    ("CLAMP_D", 49.8, "mm", "Ø extérieur du collier de serrage du M5Dial (STL Dial_v1.1 : 49,8, 12,6 de haut)"),
    ("CLAMP_GAP", 1.5, "mm", "Jeu radial autour du collier (il doit tourner librement sous l'anneau)"),
    ("CLAMP_H", 12.6, "mm", "Hauteur du collier de serrage du M5Dial (modèle officiel Dial.stl : 12,57) : point le plus bas du M5Dial"),
    ("BAT_GAP", 5.0, "mm", "Cartouche : espace entre le haut du support de batterie (remonté dans le tube) et le collier du M5Dial"),
    ("TUBE_WALL", 3.5, "mm", "Épaisseur du tube qui porte l'anneau rehaussé (et la gorge de la baïonnette de la batterie)"),
    ("LEG_ANG", 36.0, "deg", "Largeur angulaire des 3 piliers du tube (entre eux : grandes fenêtres pour serrer le collier)"),
    ("BAT_TYPE", 1, "", "Batterie : 0 = LiPo 103450 dans la coque à couvercle, 1 = cartouche 16350 amovible à clip"),
    ("BAT_L", 54.0, "mm", "Coque batterie : longueur intérieure (LiPo 103450 = 50 + carte de protection et fils)"),
    ("BAT_W", 38.5, "mm", "Coque batterie : largeur intérieure (LiPo 103450 = 34, + les 2 pattes du couvercle)"),
    ("BAT_T", 11.0, "mm", "Coque batterie : profondeur intérieure (LiPo 103450 = 10)"),
    ("SHELL_WALL", 1.6, "mm", "Coque batterie : épaisseur des parois et du fond"),
    ("BAT_ROT", 0.0, "deg", "Coque batterie : orientation sous le M5Dial (0 = grand côté dans l'axe moyeu -> M5Dial)"),
    ("BAT_TWIST", 35.0, "deg", "Coque batterie : quart de tour de la baïonnette (vissage vu de dessous)"),
    ("ENV_HOLDER", 0, "", "1 = logement de l'Unit ENV III (32 x 24 x 8) contre le tube, côté jante, glissée par le haut"),
    ("IMU_HOLDER", 0, "", "1 = logement de l'Unit IMU Pro Mini (24 x 24 x 8) sur la coque, dans le tube, à plat"),
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

    def rrect_arcs(self, cx, cy, length, width, ang, r):
        """Rectangle centré à coins en vrais arcs (tangents) : ses arêtes se congèlent ensuite d'un seul tenant."""
        sk = self.sketch()
        u = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
        n = (-u[1], u[0])
        P_ = lambda px, py: self._pt(cx + px * u[0] + py * n[0], cy + px * u[1] + py * n[1])
        a, c = length / 2 - r, width / 2 - r
        arcs = sk.sketchCurves.sketchArcs
        corners = [((a, c), 0), ((-a, c), 90), ((-a, -c), 180), ((a, -c), 270)]
        made = []
        for (px, py), a0 in corners:
            st = (px + r * math.cos(math.radians(a0)), py + r * math.sin(math.radians(a0)))
            made.append(arcs.addByCenterStartSweep(P_(px, py), P_(*st), math.pi / 2))
        lines = sk.sketchCurves.sketchLines
        for i in range(4):
            lines.addByTwoPoints(made[i].endSketchPoint, made[(i + 1) % 4].startSketchPoint)
        return sk

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
IMU_S, IMU_T = 24.0, 8.0       # Unit IMU Pro Mini (BMI270 + BMM150 + BMP280) : 24 x 24 x 8
ENV_L, ENV_W, ENV_T = 32.0, 24.0, 8.0  # Unit ENV III (SHT30 + QMP6988) : 32 x 24 x 8
HOLDER_WALL = 2.4             # parois des logements des Units
WIRE_W = 8.0                 # fente du câble de la batterie dans le dessus de la coque, depuis le bout ouvert

# Cartouche 16350 (BAT_TYPE 1) : pile de puff (Li-ion 16350, + et - aux deux bouts, languettes soudées) prisonnière
# d'une cartouche. La connexion passe AU-DESSUS de la pile (plus épais, bien plus court) par 2 embouts Dupont de
# jumper wires, côte à côte au pas de 2,54 : femelles clipsées dans un berceau sur la cartouche (douilles en retrait :
# pas de court-circuit en poche), mâles clipsés dans le support, fils remontant droit vers le M5Dial.
# Le support garde la baïonnette de la coque. Le long de la cartouche, du bout intérieur : fond 1,2 | alésage 46
# (soudures 2 + pile et languettes 39 + soudures 2 + bouchon 3) ; au-dessus, sur les 17,5 premiers mm, le bloc du
# support (embouts mâles), puis le berceau des embouts femelles et la gouttière des fils (couverte vers le bout).
# Polarité : l'embout côté +y (du côté de la fente des fils et des « + » gravés) est le +, sur la cartouche comme sur
# le support. Sans détrompeur : retournée ou bout pour bout, la cartouche bute sur la pointe des broches avant tout
# contact (ses douilles sont alors en bas ou à l'arrière) : ne pas forcer.
CELL_D = 16.9                # alésage : pile Ø16 + gaine + jeu
CART_WALL, END_WALL, MID_WALL = 2.0, 1.2, 1.4  # parois (flancs et dessous) ; fond intérieur ; plancher entre la pile et les embouts
BORE_L = 46.0
DUP_L, DUP_W, DUP_H = 14.0, 5.2, 2.6  # 2 embouts Dupont côte à côte (2,54 x 2,54 x 14) : logement serré, clipsé
DUP_WF = DUP_W + 0.5          # logement des embouts femelles de la cartouche : 0,5 de plus pour les fils Dupont
DUP_PAD = 0.9                # surélévation du berceau : les embouts mâle et femelle à la même hauteur
RECESS = 17.5                # cartouche abaissée sur ses 17,5 premiers mm : le bloc des embouts mâles s'y loge
LANE = (3.6, 7.6)            # fente des fils, sur toute la longueur : la pile y passe ses deux fils vers la gouttière (y)
CAP_T, PLUG_L = 1.6, 3.0     # bouchon : plaque, bouchon dans l'alésage
CAP_GROOVE = 0.6             # rainures des flancs de la gouttière où coulisse le couvercle du bouchon (profondeur)
CAP_DETENT = 0.35            # bossages de clic du couvercle : 0,2 de serrage au fond des rainures, puis un creux
CART_OUT = 6.0               # la cartouche dépasse du support : les bouts des bras s'y pincent
ARM_T, ARM_GAP, ARM_L = 1.5, 2.4, 26.0  # bras du clip le long des flancs : épaisseur, jeu (course pour les pincer), longueur
BUMP_P = 1.2                 # cran des bras : accroche 0,9 mm dans les logements du support (0,3 de jeu)
CLIP_IN = 2.5                # face d'accroche du cran, en retrait du bout ouvert du support (bord plein de 2,5 mm)
CLIP_D = 1.0                 # profondeur des logements borgnes des crans dans les flancs (peau de 0,6 dehors)
SPRING_D, SPRING_L = 5.4, 9.0  # logement du ressort d'éjection (ressort de stylo Ø 5 max, 14 mm et plus)
X_S = -18.0                  # bout intérieur de la cartouche, depuis le centre du M5Dial : les embouts mâles et leurs
                             # fils tombent sous la couronne, droit sous le M5Dial


def rrect(cx, cy, length, width, ang, r):
    """Contour d'un rectangle centré à coins arrondis (rayon r), côté `length` orienté selon `ang` (deg)."""
    u = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
    n = (-u[1], u[0])
    a, c = length / 2 - r, width / 2 - r
    pts = []
    for (sa, sc), a0 in (((1, 1), 0), ((-1, 1), 90), ((-1, -1), 180), ((1, -1), 270)):
        for t in range(0, 91, 15):
            ca, sb = math.cos(math.radians(a0 + t)), math.sin(math.radians(a0 + t))
            px, py = sa * a + r * ca, sc * c + r * sb
            pts.append((cx + px * u[0] + py * n[0], cy + px * u[1] + py * n[1]))
    return pts


def battery(b, P, cup, cx, cy, z_b, r_clr, a_out, z_mount):
    """Coque de la LiPo sous le tube du M5Dial (corps séparé "Coque batterie", imprimé fond sur le plateau) et,
    dans le pied du tube : encoches d'entrée, gorge et creux de verrouillage. La batterie se glisse par le bout
    ouvert de la coque (côté jante), son câble remonte par la fente jusqu'au M5Dial.
    z_b : bas de la couronne de la baïonnette (= dessous du support du volant z_mount pour la coque ; plus haut dans
    le tube pour la cartouche, dont le support remonte et tourne dans le tube)."""
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
        # Encoche d'entrée depuis le bas du tube (z_mount) : quand la couronne est remontée, les tenons y montent
        b.extrude(b.poly(sector(cx, cy, r_clr - 0.5, r_g, ae - half, ae + half)), z_mount - 1,
                  z_b - z_mount + 1 + GROOVE_Z1, CUT, [cup])
        b.extrude(b.poly(sector(cx, cy, r_clr - 0.5, r_g, ae - half, af + half)), z_b + GROOVE_Z0,
                  GROOVE_Z1 - GROOVE_Z0, CUT, [cup])
        b.extrude(b.circles([bump(af)], BUMP_D / 2 + 0.4), z_b + GROOVE_Z0 - 0.6, 0.7, CUT, [cup])

    wall, L, W, T = P["SHELL_WALL"], P["BAT_L"], P["BAT_W"], P["BAT_T"]
    cart = P["BAT_TYPE"] >= 0.5
    x_tray, l_tray = 0.0, L + 2 * wall  # coque : centrée sous le M5Dial
    z_tt = z_b                   # dessus du bac : sous la couronne (coque)
    if cart:  # support de cartouche : plus étroit, plus épais (embouts au-dessus de la pile), plus court
        wb = CELL_D + 2 * CART_WALL                             # corps de la cartouche
        tc = CART_WALL + CELL_D + MID_WALL + DUP_PAD + DUP_H    # pile, plancher, berceau des embouts
        W, T = wb + 2 * (ARM_GAP + ARM_T) + 0.6, tc + 0.6       # logement : bras compris, 0,3 de jeu par côté
        l_b = END_WALL + BORE_L                                 # longueur de la cartouche (sans bouchon)
        l_tray = l_b - CART_OUT + wall
        x0 = -l_tray / 2         # centré : il tourne dans l'alésage du tube (quart de tour de la baïonnette)
        x_stop = x0 + wall                                      # butée : bout intérieur de la cartouche
        x1 = x_stop + l_b - CART_OUT                            # bout ouvert
        x_tray = 0.0
        z_tt = z_b + BOSS_H      # dessus du bac au niveau du haut de la couronne : tout remonte de BOSS_H
    h_tray = T + 2 * wall        # fond, logement, dessus plein
    if cart:  # coins arrondis : le bac tourne dans l'alésage du tube (Ø 2 x r_clr)
        shell = b.solid(b.poly(rrect(*at(x_tray), l_tray, W + 2 * wall, ang, 3.0)), z_tt - h_tray, h_tray)
        assert math.hypot(l_tray / 2 - 3, W / 2 + wall - 3) + 3 < r_b - 0.2, "le bac ne tourne pas dans le tube"
    else:
        shell = b.solid(b.rect(*at(x_tray), l_tray, W + 2 * wall, ang), z_tt - h_tray, h_tray)
    shell.name = "Support cartouche" if cart else "Coque batterie"
    # Collerette à 45° du bord de la coque jusqu'à la couronne : imprimable sans support
    w2 = W / 2 + wall
    if r_b > w2:
        ext = b.f.extrudeFeatures
        inp = ext.createInput(b.circles([(cx, cy)], w2).profiles.item(0), NEW)
        inp.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(adsk.core.ValueInput.createByReal((r_b - w2) / 10)),
                             adsk.fusion.ExtentDirections.PositiveExtentDirection, adsk.core.ValueInput.createByString("45 deg"))
        inp.startExtent = adsk.fusion.OffsetStartDefinition.create(adsk.core.ValueInput.createByReal((z_b - (r_b - w2)) / 10))
        b.combine(shell, ext.add(inp).bodies.item(0), JOIN)
    # Couronne : anneau (évidé avant d'être joint, pour ne pas entamer le bac de la cartouche, qui monte dedans)
    crown = b.solid(b.circles([(cx, cy)], r_b), z_b, BOSS_H)
    b.extrude(b.circles([(cx, cy)], r_b - BOSS_T), z_b - 0.1, BOSS_H + 0.2, CUT, [crown])
    b.combine(shell, crown, JOIN)
    for k in LUGS:
        af = ang + k
        b.combine(shell, b.solid(b.poly(sector(cx, cy, r_b - 0.2, r_b + LUG_P, af - LUG_ANG / 2, af + LUG_ANG / 2)),
                                 z_b + LUG_Z0, LUG_Z1 - LUG_Z0), JOIN)
        b.combine(shell, b.solid(b.circles([bump(af)], BUMP_D / 2), z_b + LUG_Z0 - BUMP_H, BUMP_H), JOIN)
    v = (-u[1], u[0])
    loc = lambda px, py: (cx + px * u[0] + py * v[0], cy + px * u[1] + py * v[1])
    z_mid = z_tt - wall - T / 2
    if cart:
        FRAMES["holder"] = (cx, cy, ang, z_tt)
        holder_cuts(b, shell, loc, ang, z_tt, W, T, wall, x_stop, x1, wb, tc)
        cartridge_bodies(b, loc, ang, z_tt, T, wall, x_stop, x1, wb, tc, l_b)
        # Encoche au pied du tube : le bout de la cartouche (et ses bras à pincer) sort du tube côté jante
        top = z_tt - wall + 0.4
        b.extrude(b.rect(*loc(x1 + 6, 0), 14, W + 2, ang), z_mount - 1, top - z_mount + 1, CUT, [cup])
    else:
        lipo_tray(b, shell, at, loc, ang, z_b, z_mid, L, W, T, wall, r_b, h_tray)
    imu_holder(b, P, shell, loc, ang, z_tt, r_b)
    return shell


def lipo_tray(b, shell, at, loc, ang, z_b, z_mid, L, W, T, wall, r_b, h_tray):
    """Coque de la LiPo 103450 : logement ouvert au bout côté jante, fente du câble, aérations, couvercle à clips."""
    b.extrude(b.rect(*at((wall + 1) / 2), L + wall + 1, W, ang), z_b - wall - T, T, CUT, [shell])
    x0, x1 = r_b - BOSS_T - 6, L / 2 + wall + 1  # fente du câble du bout ouvert jusque dans la couronne
    b.extrude(b.rect(*at((x0 + x1) / 2), x1 - x0, WIRE_W, ang), z_b - wall - 0.5, wall + BOSS_H + 2, CUT, [shell])

    # Aérations : fentes dans le fond, les flancs et le dessus (dans la couronne : l'air chaud monte dans le tube)
    for py in (-12, -6, 0, 6, 12):
        b.extrude(b.rect(*loc(-2, py), 36, 3, ang), z_b - h_tray - 1, wall + 1.2, CUT, [shell])
    for px in (-18, -6, 6):
        for sy in (1, -1):
            b.extrude(b.rect(*loc(px, sy * (W / 2 + wall / 2)), 8, wall + 1, ang), z_mid - 2, 3.5, CUT, [shell])
    for py in (-10, 10):
        b.extrude(b.rect(*loc(-3, py), 22, 3, ang), z_b - wall - 0.5, wall + 1, CUT, [shell])

    # Couvercle (corps séparé "Couvercle batterie") : plaque sur le bout ouvert, 2 pattes souples le long des flancs
    # de la batterie ; leur bossage clique dans un trou du flanc (qui aère aussi). Pour l'ouvrir : appuyer sur les
    # bossages par les trous. Imprimé plaque à plat sur le plateau.
    e = L / 2 + wall                      # face extérieure du bout ouvert
    flap_l, flap_t, gap = 8.0, 1.2, 0.2   # pattes : longueur, épaisseur, jeu avec le flanc
    bump_x = e - 7                        # bossage à 7 mm de la plaque (patte assez longue pour plier)
    for sy in (1, -1):                    # trous du clic dans les flancs
        b.extrude(b.rect(*loc(bump_x, sy * (W / 2 + wall / 2)), 4, wall + 1, ang), z_mid - 2.5, 5, CUT, [shell])
    lid = b.solid(b.rect(*loc(e + 0.8, 0), 1.6, W + 2 * wall, ang), z_b - h_tray, h_tray)
    lid.name = "Couvercle batterie"
    for sy in (1, -1):
        yf = sy * (W / 2 - gap - flap_t / 2)
        b.combine(lid, b.solid(b.rect(*loc(e - flap_l / 2 + 0.05, yf), flap_l + 0.1, flap_t, ang), z_b - wall - T + 0.5, T - 1), JOIN)
        yb = sy * (W / 2 - gap + 0.2)     # bossage : dépasse de 0,4 mm la face intérieure du flanc
        b.combine(lid, b.solid(b.rect(*loc(bump_x, yb), 3, 0.8, ang), z_mid - 2, 4), JOIN)


def cyl_x(b, loc, ang, px0, py, z, r, length):
    """Cylindre couché dans l'axe de la coque : de px0 à px0 + length, axe en (py, z)."""
    c = b.solid(b.circles([(0, 0)], r), 0, length)
    b.rotate(c, (0, 1, 0), (0, 0, 0), 90)          # axe Z -> axe X
    x, y = loc(px0, py)
    b.rotate(c, (0, 0, 1), (0, 0, 0), ang, z, x, y)
    return c


def cart_levels(z_tt, T, wall, tc):
    """Hauteurs de la cartouche posée dans un logement dont le dessus du bac est à z_tt."""
    z_ct = z_tt - wall                   # plafond du logement
    z_cb = z_ct - T                      # fond du logement
    z0 = z_cb + 0.3                      # dessous de la cartouche
    z_mt = z0 + CART_WALL + CELL_D + MID_WALL  # dessus du plancher au-dessus de la pile
    z_pf = z_mt + DUP_PAD                # fond des logements d'embouts (mâles et femelles à la même hauteur)
    return dict(z_ct=z_ct, z_cb=z_cb, z0=z0, z_ax=z0 + CART_WALL + CELL_D / 2, z_mt=z_mt, z_pf=z_pf,
                z_pin=z_pf + 1.27, z_top=z0 + tc, zw=z0 + tc / 2)


# Aérations : fentes alignées sous la cartouche et dans le fond du support, et dans les flancs sur la partie
# avant (là où il n'y a pas les bras) : l'air passe autour de la pile. (x depuis la butée, y, longueur)
VENTS_BOTTOM = [(x, y, 13.0) for x in (8.0, 25.0) for y in (-1.8, 1.8)]  # centrées sous la pile
VENTS_SIDE = [(3.5, 13.0)]           # (x depuis la butée, longueur) ; 2 fentes de 1,6 mm de haut par flanc
VENT_W = 1.6


def holder_cuts(b, shell, loc, ang, z_tt, W, T, wall, x_stop, x1, wb, tc):
    """Logement d'une cartouche 16350 dans `shell` (support du volant ou chargeur) : bloc des embouts mâles,
    rainure du détrompeur, fenêtres des crans, aérations. Dessus du bac à z_tt, butée à x_stop, bout ouvert à x1."""
    L_ = cart_levels(z_tt, T, wall, tc)
    z_cb, z_ct, z_mt, z_pf, z_pin, zw = L_["z_cb"], L_["z_ct"], L_["z_mt"], L_["z_pf"], L_["z_pin"], L_["zw"]
    xr = x_stop + RECESS                 # fin de l'abaissement de la cartouche = face avant du berceau femelle
    # Logement : pleine hauteur à partir du berceau ; au-dessus de l'abaissement de la cartouche, le bloc des mâles
    # (prolongé au-delà du bout ouvert : la cartouche qui dépasse traverse la couronne et la collerette, montées
    # à sa hauteur quand le support remonte dans le tube)
    b.extrude(b.rect(*loc((x_stop + x1 + 12) / 2, 0), x1 + 12 - x_stop, W, ang), z_cb, z_mt + 0.3 - z_cb, CUT, [shell])
    b.extrude(b.rect(*loc((xr - 0.2 + x1 + 12) / 2, 0), x1 + 12.2 - xr, W, ang), z_mt + 0.3, z_ct - z_mt - 0.3, CUT, [shell])
    # Bloc : 2 embouts mâles côte à côte, posés par le dessus dans un logement serré (clipsés, un point de colle au
    # besoin) ; broches à travers un rebord de 0,6 vers la cartouche ; derrière eux une marche de 0,6 encaisse la
    # poussée et les fils remontent droit (vers le M5Dial, ou le TP4056 du chargeur).
    xm1 = xr - 0.8                       # face avant des embouts mâles (contre le rebord)
    xm0 = xm1 - DUP_L
    b.extrude(b.rect(*loc((xm0 + xm1) / 2, 0), DUP_L, DUP_W, ang), z_pf, z_tt + 1 - z_pf, CUT, [shell])
    b.extrude(b.rect(*loc(xm0 - 1.5, 0), 3.0, DUP_W, ang), z_pf + 0.6, z_tt + 1 - z_pf - 0.6, CUT, [shell])
    b.extrude(b.rect(*loc(xr - 0.5, 0), 0.9, 4.4, ang), z_pin - 0.6, 1.2, CUT, [shell])
    # « + » gravé à côté de l'embout + (côté +y), sur le dessus du bac
    for lx, ly in ((3.0, 0.7), (0.7, 3.0)):
        b.extrude(b.rect(*loc(xm0 + 5, 5.5), lx, ly, ang), z_tt - 0.4, 1.4, CUT, [shell])
    # Ressort d'éjection : logement dans un bossage derrière le fond, au centre du bout intérieur de la cartouche ;
    # comprimé quand elle est clipsée (il la plaque contre les crans), il la repousse quand on pince les bras.
    # Le bossage descend jusqu'au fond (imprimable sans support) ; sur le volant il reste sous le support.
    z_ax = L_["z_ax"]
    xbk = x_stop - wall
    b.combine(shell, b.solid(b.rect(*loc(xbk - (SPRING_L - wall + 1) / 2 + 0.5, 0), SPRING_L - wall + 2, SPRING_D + 3.6, ang),
                             z_cb - wall, z_ax + SPRING_D / 2 + 1.8 - (z_cb - wall)), JOIN)
    b.combine(shell, cyl_x(b, loc, ang, x_stop - SPRING_L, 0, z_ax, SPRING_D / 2, SPRING_L + 0.2), CUT)
    # Logements des crans : borgnes (la paroi reste fermée dehors, le cran accroche un bord plein de CLIP_IN mm et non
    # plus un fin barreau), bord d'accroche exactement sur la face d'accroche du cran ; le cran est en légère dépouille,
    # le ressort du bras plaque la cartouche contre sa butée : pas de jeu en longueur
    for sy in (1, -1):
        b.extrude(b.rect(*loc(x1 - CLIP_IN - 2.2, sy * (W / 2 + (CLIP_D - 0.1) / 2)), 4.4, CLIP_D + 0.1, ang), zw - 4, 8, CUT, [shell])
    # Aérations du fond (face aux fentes de la cartouche) et des flancs
    for vx, vy, vl in VENTS_BOTTOM:
        b.extrude(b.poly(rrect(*loc(x_stop + vx + vl / 2, vy), vl, VENT_W, ang, VENT_W / 2 - 0.01)),
                  z_cb - wall - 0.5, wall + 1, CUT, [shell])
    for vx, vl in VENTS_SIDE:
        for sy in (1, -1):
            for dz in (-3.0, 3.0):
                b.extrude(b.rect(*loc(x_stop + vx + vl / 2, sy * (W / 2 + wall / 2)), vl, wall + 1, ang),
                          L_["z_ax"] + dz - VENT_W / 2, VENT_W, CUT, [shell])


def edges_along(body, u, zsel):
    """Arêtes rectilignes du corps parallèles à u (horizontales), dont la hauteur (mm) vérifie zsel(z)."""
    out = []
    for e in body.edges:
        g = e.geometry
        if not isinstance(g, adsk.core.Line3D):
            continue
        p, q = g.startPoint, g.endPoint
        d = (q.x - p.x, q.y - p.y, q.z - p.z)
        ln = math.sqrt(sum(c * c for c in d))
        if ln > 1e-6 and abs(d[0] * u[0] + d[1] * u[1]) / ln > 0.999 and abs(d[2]) < 1e-6 and zsel(p.z * 10):
            out.append(e)
    return out


def cartridge_bodies(b, loc, ang, z_tt, T, wall, x_stop, x1, wb, tc, l_b):
    """Cartouche 16350 posée dans son logement (corps "Cartouche 16350") et son bouchon ("Bouchon cartouche").
    La cartouche entre par le bout ouvert ; ses deux bras à cran cliquent dans les fenêtres des flancs ; on les
    pince (leurs bouts dépassent) pour la ressortir. En fin de course, les broches des 2 embouts Dupont mâles (bloc
    du support) entrent dans les 2 embouts femelles (berceau de la cartouche).
    Fils : une fente ouverte sur toute la longueur relie l'alésage à la gouttière du dessus ; les deux fils de la pile y
    montent sans trou à enfiler et suivent la gouttière jusqu'aux embouts ; le couvercle du bouchon les cache.
    Arrondis : grands rayons dessous (4 mm), dessus adouci (1 mm), bouchon assorti."""
    L_ = cart_levels(z_tt, T, wall, tc)
    z_cb, z0, z_ax, z_mt, z_pf, z_pin, z_top, zw = (L_[k] for k in ("z_cb", "z0", "z_ax", "z_mt", "z_pf", "z_pin", "z_top", "zw"))
    u = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
    xr = x_stop + RECESS
    xe = x_stop + l_b                    # bout extérieur de la cartouche (sans bouchon)
    cart = b.solid(b.rect(*loc((x_stop + xe) / 2, 0), l_b, wb, ang), z0, tc)
    cart.name = "Cartouche 16350"
    b.fillet(edges_along(cart, u, lambda z: abs(z - z0) < 1e-3), 4.0, False)
    b.fillet(edges_along(cart, u, lambda z: abs(z - z_top) < 1e-3), 1.0, False)
    # Bras du clip : attachés côté intérieur par un pont, libres vers l'extérieur ; cran à rampe (côté entrée) juste
    # avant le bout ouvert du support, face d'accroche en légère dépouille ; leurs 6 derniers mm dépassent : on les
    # pince pour sortir la cartouche.
    xa = xe - ARM_L
    for sy in (1, -1):
        ya = sy * (wb / 2 + ARM_GAP + ARM_T / 2)
        # Pont d'abord, puis le bras qui le touche : joint à part, un bras isolé resterait un corps séparé
        b.combine(cart, b.solid(b.rect(*loc(xa + 1.25, sy * (wb / 2 + ARM_GAP / 2)), 2.5, ARM_GAP + 0.6, ang), z0 + 1, tc - 2), JOIN)
        b.combine(cart, b.solid(b.rect(*loc((xa + xe) / 2, ya), ARM_L, ARM_T, ang), z0 + 1, tc - 2), JOIN)
        yo, xh = sy * (wb / 2 + ARM_GAP + ARM_T), x1 - CLIP_IN  # face extérieure du bras, face d'accroche du cran
        bump = [loc(xh - 3.6, yo - sy * 0.1), loc(xh - 0.9, yo + sy * BUMP_P), loc(xh - 0.3, yo + sy * BUMP_P), loc(xh, yo - sy * 0.1)]
        b.combine(cart, b.solid(b.poly(bump), zw - 3, 6), JOIN)
    # Abaissement sous le bloc du support, puis gouttière ouverte sur toute la largeur intérieure
    b.extrude(b.rect(*loc((x_stop - 0.5 + xr) / 2, 0), xr - x_stop + 0.5, wb + 1, ang), z_mt, tc, CUT, [cart])
    b.extrude(b.rect(*loc((xr + xe + 1) / 2, 0), xe + 1 - xr, wb - 2 * CART_WALL, ang), z_mt, tc, CUT, [cart])
    # Berceau des 2 embouts femelles : flancs de 1,5, rebord avant de 0,6 (fenêtre des douilles), logement serré,
    # marche arrière de 0,6 (les fils passent dessus) ; posés par le dessus, le plafond du support les tient en place
    xf0 = xr + 0.6                       # face avant des embouts femelles
    xf1 = xf0 + DUP_L
    cr = b.solid(b.rect(*loc((xr + xf1 + 0.8) / 2, 0), xf1 + 0.8 - xr, DUP_WF + 3.0, ang), z_mt - 0.1, z_top - z_mt + 0.1)
    b.combine(cart, cr, JOIN)
    b.extrude(b.rect(*loc((xf0 + xf1) / 2, 0), DUP_L, DUP_WF, ang), z_pf, tc, CUT, [cart])
    b.extrude(b.rect(*loc(xf1 + 0.45, 0), 0.9, DUP_WF, ang), z_pf + 0.6, tc, CUT, [cart])
    b.extrude(b.rect(*loc(xr + 0.25, 0), 0.9, 4.4, ang), z_pin - 0.6, 1.2, CUT, [cart])
    # Alésage de la pile, fente des fils sur toute la longueur (plancher ouvert côté +y), et rainures des flancs de la
    # gouttière (du virage des fils au bout) où coulisse le couvercle du bouchon, avec les creux de son clic
    b.combine(cart, cyl_x(b, loc, ang, x_stop + END_WALL, 0, z_ax, CELL_D / 2, BORE_L + 1), CUT)
    b.extrude(b.rect(*loc((x_stop + END_WALL + 0.2 + xe + 1) / 2, (LANE[0] + LANE[1]) / 2), xe + 1 - x_stop - END_WALL - 0.2,
                     LANE[1] - LANE[0], ang), z_mt - MID_WALL - 0.3, MID_WALL + 0.4, CUT, [cart])
    # Au bout éloigné, la fente s'élargit sur 7,2 mm et descend 2,5 mm plus bas, le long de la pile : le fil du bout de
    # la pile y remonte vers la gouttière facilement
    b.extrude(b.rect(*loc(xe - 3.1, (1.5 + LANE[1] + 0.4) / 2), 8.2, LANE[1] + 0.4 - 1.5, ang), z_mt - MID_WALL - 2.8,
              MID_WALL + 2.9, CUT, [cart])
    xcv = xf1 + 0.8 + 3                  # début du couvercle : fin du berceau + 3 mm pour le virage des fils
    yi = wb / 2 - CART_WALL              # flanc intérieur de la gouttière
    b.extrude(b.rect(*loc((xcv - 0.5 + xe + 1) / 2, 0), xe + 1.5 - xcv, 2 * (yi + CAP_GROOVE), ang), z_top - 1.7, 1.2, CUT, [cart])
    b.extrude(b.rect(*loc(xe - 3.75, 0), 2.1, 2 * (yi + CAP_GROOVE - 0.15 + CAP_DETENT + 0.05), ang), z_top - 1.7, 1.2, CUT, [cart])
    # Aérations : fentes sous la pile (face à celles du support) et dans les flancs avant
    for vx, vy, vl in VENTS_BOTTOM:
        b.extrude(b.poly(rrect(*loc(x_stop + vx + vl / 2, vy), vl, VENT_W, ang, VENT_W / 2 - 0.01)),
                  z0 - 0.5, CART_WALL + 1.0, CUT, [cart])
    for vx, vl in VENTS_SIDE:
        for sy in (1, -1):
            for dz in (-3.0, 3.0):
                b.extrude(b.rect(*loc(x_stop + vx + vl / 2, sy * wb / 2), vl, CART_WALL * 2 + 1, ang),
                          z_ax + dz - VENT_W / 2, VENT_W, CUT, [cart])

    # --- Bouchon : plaque au bout (mêmes arrondis), bouchon serré dans l'alésage, et couvercle qui coulisse dans les
    # rainures de la gouttière jusqu'aux embouts : il cache les fils, et ses 2 bossages cliquent dans les creux ---
    cap = b.solid(b.rect(*loc(xe + CAP_T / 2, 0), CAP_T, wb, ang), z0, tc)
    cap.name = "Bouchon cartouche"
    b.fillet(edges_along(cap, u, lambda z: abs(z - z0) < 1e-3), 4.0, False)
    b.fillet(edges_along(cap, u, lambda z: abs(z - z_top) < 1e-3), 1.0, False)
    b.combine(cap, cyl_x(b, loc, ang, xe - PLUG_L, 0, z_ax, CELL_D / 2 - 0.05, PLUG_L + 0.1), JOIN)
    # Encoche du bouchon côté +y, sous la fente : le fil du bout de la pile passe le long du bouchon pour remonter
    b.extrude(b.rect(*loc(xe - PLUG_L / 2 - 0.2, (1.5 + CELL_D / 2 + 0.5) / 2), PLUG_L + 0.5, CELL_D / 2 + 0.5 - 1.5, ang),
              z_ax + 2.0, CELL_D / 2 + 1.0, CUT, [cap])
    yt = yi + CAP_GROOVE - 0.15          # bord des languettes (0,15 de jeu au fond des rainures)
    b.combine(cap, b.solid(b.rect(*loc((xcv + 0.3 + xe + 0.1) / 2, 0), xe + 0.1 - xcv - 0.3, 2 * yt, ang), z_top - 1.6, 1.0), JOIN)
    for sy in (1, -1):
        b.combine(cap, b.solid(b.rect(*loc(xe - 3.75, sy * (yt + CAP_DETENT / 2 - 0.05)), 1.5, CAP_DETENT + 0.1, ang),
                               z_top - 1.4, 0.6), JOIN)
    # « + » gravé sur le couvercle, côté +y
    for lx, ly in ((3.0, 0.7), (0.7, 3.0)):
        b.extrude(b.rect(*loc(xe - 6, 5.5), lx, ly, ang), z_top - 0.9, 0.5, CUT, [cap])
    return cart, cap


# Chargeur de cartouche (BAT_TYPE 1) : socle posé sur la table, même logement que le support du volant (détrompeur,
# clip, embouts mâles), et au-dessus un compartiment pour un module TP4056 (carte 28 x 17,5, USB-C ou micro-USB) :
# les 2 embouts mâles remontent dans le compartiment, leurs fils vont aux pastilles B+ / B- du module.
DOCK_AT = (0.0, -150.0)      # place du chargeur dans le document Fusion (à l'écart du support du volant)
BAY_IN_W = 17.6              # compartiment : largeur intérieure (TP4056 26 x 17 + jeu)
BAY_POST = 3.0               # rails sous la carte : les fils des embouts passent dessous
BAY_H = 10.5                 # hauteur des parois du compartiment au-dessus du bac
USB_W, USB_H = 9.2, 3.3      # prise USB-C du TP4056 : encoche à sa taille, à ras de la face arrière
USB_OVH = 0.6                # la prise dépasse du bord de la carte de 0,6 : la paroi arrière n'a que 0,6 là où la
                             # carte s'appuie (ajuster si la prise affleure mal)
LID_T, LIP_H, LIP_T = 1.6, 2.5, 1.2  # couvercle : plaque, rebord emboîté (0,2 de jeu), épaisseur du rebord
FRAMES = {}                  # repères (cx, cy, angle, dessus du bac) du support et du chargeur, pour les contrôles


def cart_dims(P):
    """Cotes communes cartouche / logement : corps, logement, longueurs, positions (repère du bac centré)."""
    wall = P["SHELL_WALL"]
    wb = CELL_D + 2 * CART_WALL
    tc = CART_WALL + CELL_D + MID_WALL + DUP_PAD + DUP_H
    W, T = wb + 2 * (ARM_GAP + ARM_T) + 0.6, tc + 0.6
    l_b = END_WALL + BORE_L
    l_tray = l_b - CART_OUT + wall
    x0 = -l_tray / 2
    x_stop = x0 + wall
    x1 = x_stop + l_b - CART_OUT
    return dict(wall=wall, wb=wb, tc=tc, W=W, T=T, l_b=l_b, l_tray=l_tray, x0=x0, x_stop=x_stop, x1=x1)


def dock(b, P):
    """Chargeur ("Chargeur cartouche" + "Couvercle chargeur"), posé à DOCK_AT, fond à z = 0."""
    d = cart_dims(P)
    wall, W, T, l_tray, x0, x_stop, x1 = d["wall"], d["W"], d["T"], d["l_tray"], d["x0"], d["x_stop"], d["x1"]
    ox, oy = DOCK_AT
    loc = lambda px, py: (ox + px, oy + py)
    h_tray = T + 2 * wall
    z_tt = h_tray
    FRAMES["dock"] = (ox, oy, 0.0, z_tt)
    body = b.solid(b.rrect_arcs(ox, oy, l_tray, W + 2 * wall, 0, 3.0), 0, h_tray)
    body.name = "Chargeur cartouche"
    # Semelle : plus large (stable), arrêtée au bout ouvert (la nervure de la cartouche passe dessous)
    b.combine(body, b.solid(b.rrect_arcs(*loc((x0 - 8 + x1) / 2, 0), x1 - x0 + 8, W + 2 * wall + 16, 0, 6.0), 0, wall), JOIN)
    # Compartiment du TP4056 sur le dessus (parois arrondies), rails sous la carte
    bw = BAY_IN_W + 2 * wall
    b.combine(body, b.solid(b.rrect_arcs(ox, oy, l_tray, bw, 0, 3.0), z_tt - 0.1, BAY_H + 0.1), JOIN)
    b.extrude(b.rrect_arcs(ox, oy, l_tray - 2 * wall, BAY_IN_W, 0, 1.5), z_tt, BAY_H + 1, CUT, [body])
    for sy in (1, -1):
        b.combine(body, b.solid(b.rect(*loc(0, sy * (BAY_IN_W / 2 - 0.6)), l_tray - 2 * wall, 1.2, 0), z_tt - 0.1, BAY_POST + 0.1), JOIN)
    # Paroi arrière amincie de l'intérieur à USB_OVH là où la carte s'appuie : son bord y entre, et sa prise USB-C
    # arrive à ras de la face extérieure par une encoche à sa taille (ouverte en haut : la carte se pose d'en haut)
    z_pcb = z_tt + BAY_POST + 1.6        # dessus de la carte
    b.extrude(b.rect(*loc(x0 + USB_OVH + (wall - USB_OVH) / 2 + 0.05, 0), wall - USB_OVH + 0.1, 17.4, 0), z_tt + BAY_POST - 0.2,
              BAY_H + 1, CUT, [body])
    b.extrude(b.rect(*loc(x0 + USB_OVH / 2, 0), USB_OVH + 0.4, USB_W, 0), z_pcb - 0.1, z_tt + BAY_H - z_pcb + 1, CUT, [body])
    # Logement de la cartouche (mêmes découpes que le support du volant) : le bloc des embouts s'ouvre dans le
    # compartiment ; le dessus du bac sert de fond au compartiment
    holder_cuts(b, body, loc, 0.0, z_tt, W, T, wall, x_stop, x1, d["wb"], d["tc"])

    # Couvercle : plaque arrondie, rebord emboîté (0,2 de jeu, sauf face à l'USB), fenêtre des LED de charge
    zt = z_tt + BAY_H
    lid = b.solid(b.rrect_arcs(ox, oy, l_tray, bw, 0, 3.0), zt, LID_T)
    lid.name = "Couvercle chargeur"
    li, wi = l_tray - 2 * wall - 0.4, BAY_IN_W - 0.4
    b.combine(lid, b.solid(b.rrect_arcs(ox, oy, li, wi, 0, 1.3), zt - LIP_H, LIP_H + 0.1), JOIN)
    b.extrude(b.rrect_arcs(ox, oy, li - 2 * LIP_T, wi - 2 * LIP_T, 0, 0.6), zt - LIP_H - 0.1, LIP_H + 0.1, CUT, [lid])
    b.extrude(b.rect(*loc(x0 + wall + 1, 0), 4, 17.6, 0), zt - LIP_H - 0.2, LIP_H + 0.1, CUT, [lid])
    # Languette qui comble l'encoche au-dessus de la prise (face arrière lisse) et appuie sur elle : la carte est tenue
    b.combine(lid, b.solid(b.rect(*loc(x0 + wall / 2, 0), wall, USB_W - 0.3, 0), z_pcb + USB_H + 0.1, zt - z_pcb - USB_H), JOIN)
    b.extrude(b.poly(rrect(*loc(x0 + wall + 10, 0), 12, 5, 0, 2.49)), zt - 0.1, LID_T + 0.2, CUT, [lid])
    return body, lid


def imu_holder(b, P, shell, loc, ang, z_b, r_b):
    """Logement de l'IMU Pro Mini sur le dessus de la coque, dans la couronne : à plat (parallèle à l'écran, ce que
    demande "toujours droit"), sous le M5Dial, câble court vers le Port A. Ouvert côté jante : l'unité s'y glisse
    (prise Grove de ce côté), deux rebords la tiennent par-dessus.
    Parois de 2,4 mm, rebords de 2 x 1,6 mm, et nervures qui l'ancrent à la couronne (posée, elle, sur la
    collerette pleine) : le dessus de la coque n'est qu'un pont de 1,6 mm au-dessus de la batterie."""
    if P["IMU_HOLDER"] >= 0.5:
        s_, hc, hw = IMU_S + 0.4, -2.0, HOLDER_WALL
        h_box = IMU_T + 0.4 + 1.6
        b.combine(shell, b.solid(b.rect(*loc(hc, 0), s_ + 2 * hw, s_ + 2 * hw, ang), z_b, h_box), JOIN)
        r_ring = r_b - BOSS_T + 0.5   # un peu dans la couronne
        for px in (hc - 7, hc + 5):   # 2 nervures de chaque côté, jusqu'à la couronne
            y0, y1 = s_ / 2 + hw - 0.5, math.sqrt(r_ring ** 2 - px ** 2)
            for sy in (1, -1):
                b.combine(shell, b.solid(b.rect(*loc(px, sy * (y0 + y1) / 2), 2.4, y1 - y0, ang), z_b, BOSS_H), JOIN)
        x0 = hc - s_ / 2 - hw + 0.5   # et une derrière
        b.combine(shell, b.solid(b.rect(*loc((x0 - r_ring) / 2, 0), x0 + r_ring, 2.4, ang), z_b, BOSS_H), JOIN)
        b.extrude(b.rect(*loc(hc + (hw + 1) / 2, 0), s_ + hw + 1, s_, ang), z_b, IMU_T + 0.4, CUT, [shell])
        b.extrude(b.rect(*loc(hc + (hw + 1) / 2, 0), s_ + hw + 1, s_ - 4, ang), z_b + IMU_T + 0.3, 2, CUT, [shell])


def env_holder(b, cup, cx, cy, z_b, r_base, a_out):
    """Logement de l'Unit ENV III contre la paroi extérieure du tube, côté jante : à l'air libre, loin de la
    batterie. L'unité (32 x 24 x 8, grand côté à l'horizontale) se glisse par le haut ; deux nervures la serrent,
    la face avant est ajourée et les deux bouts sont ouverts pour la prise Grove. Renvoie sa demi-largeur angulaire."""
    w = (math.cos(math.radians(a_out)), math.sin(math.radians(a_out)))
    at_r = lambda r, t: (cx + w[0] * r - w[1] * t, cy + w[1] * r + w[0] * t)  # (rayon, décalage tangentiel)
    ang_t = a_out + 90
    L_, H_, T_ = ENV_L + 0.4, ENV_W + 0.4, ENV_T + 0.2    # logement : tangentiel, hauteur, profondeur
    hw = HOLDER_WALL                                      # parois, fond et face avant
    r0, rs = r_base - 8, r_base + 0.5                     # arrière noyé dans la paroi du tube sur toute la largeur
    r1 = rs + T_ + hw                                     # face avant
    b.combine(cup, b.solid(b.rect(*at_r((r0 + r1) / 2, 0), L_ + 2 * hw, r1 - r0, ang_t), z_b, H_ + hw), JOIN)
    b.extrude(b.rect(*at_r(rs + T_ / 2, 0), L_, T_, ang_t), z_b + hw, H_ + 1, CUT, [cup])
    b.extrude(b.rect(*at_r(rs + T_ + hw / 2, 0), L_ - 12, hw + 2, ang_t), z_b + hw + 4, H_, CUT, [cup])  # rebords de 6 mm
    for sgn in (1, -1):  # bouts ouverts au-dessus de 8 mm (prise Grove) et nervures de serrage sur le fond
        b.extrude(b.rect(*at_r(rs + T_ / 2, sgn * (L_ / 2 + hw / 2)), hw + 2, T_, ang_t), z_b + hw + 8, H_, CUT, [cup])
        b.combine(cup, b.solid(b.rect(*at_r(rs + 0.2, sgn * 8), 1.0, 0.4, ang_t), z_b + hw, H_), JOIN)
    return math.degrees(math.asin(min(1.0, (L_ / 2 + hw + 2) / r_base))) + 4


def free_runs(busy):
    """Plages libres autour du tube : [(début en degrés, largeur en degrés)] hors des degrés occupés (modulo 360)."""
    occ = [False] * 360
    for v in busy:
        occ[v % 360] = True
    if all(occ) or not any(occ):
        return [] if all(occ) else [(0, 360)]
    start, runs, cur = occ.index(True), [], None
    for i in range(1, 361):
        d = start + i
        if not occ[d % 360]:
            cur = [d, 1] if cur is None else [cur[0], cur[1] + 1]
        elif cur:
            runs.append(tuple(cur))
            cur = None
    return runs


assert free_runs(set(range(350, 371)) | set(range(100, 120))) == [(11, 89), (120, 230)]


def build(comp, P):
    b = Builder(comp)
    cx, cy = P["DEV_X"], P["DEV_Y"]
    r_out = P["DIAL_D"] / 2
    # Plan du plateau (= dos de la plaque, où se colle la batterie) : sous le bout le plus bas des coins
    z_b = min(P[k + "_DZ"] - P["PAD_L"] / 2 * math.sin(math.radians(P[k + "_TILT"])) for k in ("R", "B")) - P["PAD_MIN"]
    test = P["TEST_FIT"] >= 0.5  # essai d'angle : mêmes pattes et même plaque entre elles, sans la partie M5Dial
    slab = P["ARM_T"]  # pièce d'essai : plaque entre les pattes ; support complet : disque caché dans le tube
    z_ring = z_b + P["DIAL_RISE"]  # bas de l'anneau qui tient le M5Dial
    raised = P["DIAL_RISE"] > 9
    r_clr = P["CLAMP_D"] / 2 + P["CLAMP_GAP"]  # vide sous l'anneau rehaussé : le collier s'y visse
    r_base = r_clr + P["TUBE_WALL"] if raised else r_out
    # Baïonnette : au pied du tube pour la coque LiPo ; pour la cartouche, le support remonte dans le tube jusqu'à
    # BAT_GAP sous le collier du M5Dial (son point le plus bas, CLAMP_H sous l'anneau) : il pend d'autant moins.
    up = P["BAT_TYPE"] >= 0.5 and raised and not test
    z_cr = max(z_b, z_ring - P["CLAMP_H"] - P["BAT_GAP"] - BOSS_H) if up else z_b  # bas de la couronne
    z_win = z_cr + GROOVE_Z1 + 1.2  # bas des fenêtres du tube, au-dessus de la gorge de la baïonnette

    # Pièce d'essai : plaque d'un seul contour convexe entre les côtés intérieurs des deux pattes. Support complet :
    # les pattes prolongées rejoignent le tube, plus de plaque (elle laissait des languettes au pied du tube) ;
    # le corps de départ est un disque caché dans le tube.
    rp = r_base - 1
    pts = [] if test else [(cx + rp * math.cos(math.radians(a)), cy + rp * math.sin(math.radians(a))) for a in range(0, 360, 2)]
    for k in ("R", "B") if test else ():
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
    busy = set()  # degrés (autour du M5Dial) où le tube doit rester plein
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
        hl0 = P["PAD_L"] / 2 * math.cos(math.radians(til))
        # Bout côté jante du coin : face penchée qui avance vers la jante jusqu'au pied (z_b)
        depth = (P["PAD_L"] / 2 * math.sin(math.radians(til)) + dz - z_b) / math.cos(math.radians(til))
        t_max = hl0 + depth * math.sin(math.radians(til))
        for d in range(360):  # degrés du tube touchés par la patte prolongée (+ 4° de marge) : pas de fenêtre là
            px, py = cx + r_base * math.cos(math.radians(d)) - x, cy + r_base * math.sin(math.radians(d)) - y
            if -hl0 <= ux * px + uy * py <= t_max and -pw / 2 <= sd * (nx * px + ny * py) <= pw / 2 + join:
                busy.update(range(d - 4, d + 5))
        hl = P["PAD_L"] / 2 * math.cos(math.radians(til)) + 15
        b.combine(cup, b.solid(b.rect(x - hl * ux + ox, y - hl * uy + oy, 30, pw + join + 4, rad), z_b - 1, 100), CUT)
        # Rien au-dessus de la face arrière de la branche (elle s'élargit vers le moyeu)
        b.combine(cup, tilted(b.rect(x, y, 70, band_w(P, k), rad), 0, 40), CUT)
    if not test:  # tube ajouté après les recoupes des pattes, qui ne doivent pas l'entailler
        b.combine(cup, b.solid(b.circles([(cx, cy)], r_base), z_b, max(slab, P["DIAL_RISE"] + P["RING_T"])), JOIN)
        if raised and P["ENV_HOLDER"] >= 0.5:
            half_env = env_holder(b, cup, cx, cy, z_b, r_base, a_out)
            busy.update(range(int(a_out - half_env), int(a_out + half_env) + 1))  # pas de fenêtre derrière

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
    # Les fenêtres évitent aussi l'endroit où chaque patte rejoint le tube (sinon : ailettes et entailles).
    if raised and not test:
        legs = [math.degrees(math.atan2(P[k + "_Y"] - cy, P[k + "_X"] - cx)) for k in ("R", "B")] + [a_out]
        for a in legs:
            busy.update(range(int(round(a - P["LEG_ANG"] / 2)), int(round(a + P["LEG_ANG"] / 2)) + 1))
        h = z_ring - z_win  # hauteur de fenêtre : le haut de l'arche touche le dessous de l'anneau
        for a0, span in free_runs(busy):
            ww = min(2 * r_clr * math.sin(math.radians(span / 2)), 2 * (h - 3))  # largeur à l'intérieur du tube
            if ww < 12:
                continue
            ra = ww / 2
            pts = ([(ra, 0), (ra, h - ra)] + [(ra * math.cos(math.radians(t)), h - ra + ra * math.sin(math.radians(t)))
                                               for t in range(5, 180, 5)] + [(-ra, h - ra), (-ra, 0)])
            win = b.solid(b.poly(pts), 0, r_base + 5)  # arche dressée dans le plan XZ, poussée selon -Y
            b.rotate(win, (1, 0, 0), (0, 0, 0), 90)
            b.rotate(win, (0, 0, 1), (0, 0, 0), a0 + span / 2 + 90, z_win, cx, cy)
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
        battery(b, P, cup, cx, cy, z_cr, r_clr, a_out, z_b)
        if up:
            dock(b, P)
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
