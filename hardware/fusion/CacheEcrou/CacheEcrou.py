"""Fusion 360 : caches pour les branches du volant (côté conducteur). Source de hardware/cache_ecrou.stl et
hardware/cache_factice.stl.

- Cache d'écrou (x2) : dôme bas, base plate posée sur la face de la branche, emboîté serré sur l'écrou M5
  (empreinte hexagonale). Avec ou sans rondelle large Ø 20 : sans rondelle, le bas de l'écrou est dans le lamage
  de la rondelle et l'hexagone le tient plus haut. Vis M5 x 14 : son bout entre dans le trou central.
  Imprimé base sur le plateau, sans support.
- Cache factice (x1) : même dôme, plein, avec un téton fendu en 4 qui se clipse dans le grand trou (Ø 16) de la
  branche de gauche, où rien n'est fixé : les trois branches se ressemblent. Imprimé téton sur le plateau, avec
  des supports sous le bord du dôme.

Lancer : Utilitaires > Compléments > Scripts > "+" > dossier CacheEcrou > Exécuter. Crée (ou refait) le
document "Cache ecrou M5".
"""
import math
import traceback

import adsk.core
import adsk.fusion

DOC = "Cache ecrou M5"
R = 12.0           # rayon de la base (couvre la rondelle Ø 20,6 avec 1,7 mm de paroi)
H = 7.5            # hauteur totale du dôme (bas : juste l'écrou, sa rondelle et 0,5 mm de vis)
H_SKIRT = 1.5      # bord droit au pied du dôme avant l'arrondi
WASHER_D = 20.6    # lamage : rondelle large Ø 20 (+ jeu)
WASHER_T = 1.8     # épaisseur rondelle 1,5 + jeu
NUT_AF = 8.1       # écrou M5 = 8,0 sur plats : 8,1 = serré une fois imprimé (8,0 si trop lâche, 8,2 si trop dur)
NUT_T = 4.2        # écrou M5 = 4 mm
SCREW_D = 5.6      # bout de la vis M5 qui dépasse de l'écrou
SCREW_TOP = 6.3    # 1,2 mm de plastique au sommet (vis M5 x 14 : 0,5 mm au-dessus de l'écrou)
HOLE_D = 16.0      # grand trou de la branche (cache factice)
SPOKE_T = 5.0      # épaisseur de la branche
PEG_WALL, BARB = 1.2, 0.5  # téton du cache factice : paroi des 4 doigts, saillie du crochet sous la branche

# Arc du dôme : centre sur l'axe, passe par (R, H_SKIRT) et (0, H)
ZC = (H * H - H_SKIRT * H_SKIRT - R * R) / (2 * (H - H_SKIRT))
RA = H - ZC


def dome_x(z):
    """Rayon extérieur du dôme à la hauteur z (mm)."""
    return R if z <= H_SKIRT else math.sqrt(max(0.0, RA * RA - (z - ZC) ** 2))


# Parois minimales (mm) : autour de l'écrou, du bout de vis et du lamage ; doigts du téton assez souples
assert dome_x(WASHER_T + NUT_T) - NUT_AF / math.sqrt(3) > 1.5
assert dome_x(SCREW_TOP) - SCREW_D / 2 > 1.5 and H - SCREW_TOP >= 1.2
assert R - WASHER_D / 2 >= 1.6
assert 3 * PEG_WALL * BARB / (2 * (SPOKE_T + 1.7) ** 2) < 0.025  # déformation du doigt au clipsage < 2,5 %


def p3(x, y, z=0.0):
    return adsk.core.Point3D.create(x / 10, y / 10, z / 10)


def revolve(comp, ox, prof_pts, op, body=None):
    """Révolution autour de l'axe vertical passant par (ox, 0) d'un profil (r, z) fermé par des segments."""
    f = comp.features
    sk = comp.sketches.add(comp.xZConstructionPlane)
    s = lambda r, z: sk.modelToSketchSpace(p3(ox + r, 0, z))
    L = sk.sketchCurves.sketchLines
    pts = [s(r, z) for r, z in prof_pts]
    for a, b in zip(pts, pts[1:] + pts[:1]):
        L.addByTwoPoints(a, b)
    axis = L.addByTwoPoints(s(0, -20), s(0, 20))
    axis.isConstruction = True
    rin = f.revolveFeatures.createInput(sk.profiles.item(0), axis, op)
    rin.setAngleExtent(False, adsk.core.ValueInput.createByString("360 deg"))
    if body is not None:
        rin.participantBodies = [body]
    return f.revolveFeatures.add(rin)


def dome(comp, ox, name):
    f = comp.features
    sk = comp.sketches.add(comp.xZConstructionPlane)
    s = lambda x, z: sk.modelToSketchSpace(p3(ox + x, 0, z))
    L = sk.sketchCurves.sketchLines
    a = L.addByTwoPoints(s(0, 0), s(R, 0))
    b = L.addByTwoPoints(a.endSketchPoint, s(R, H_SKIRT))
    zm = ZC + RA * math.sin(math.radians(60))  # point intermédiaire de l'arc
    arc = sk.sketchCurves.sketchArcs.addByThreePoints(b.endSketchPoint, s(math.sqrt(RA * RA - (zm - ZC) ** 2), zm), s(0, H))
    # Un arc de Fusion va toujours dans le sens trigo : son extrémité sur l'axe peut être le début ou la fin
    top = min((arc.startSketchPoint, arc.endSketchPoint), key=lambda p: abs(sk.sketchToModelSpace(p.geometry).x - ox / 10))
    axis = L.addByTwoPoints(top, a.startSketchPoint)
    rin = f.revolveFeatures.createInput(sk.profiles.item(0), axis, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
    rin.setAngleExtent(False, adsk.core.ValueInput.createByString("360 deg"))
    body = f.revolveFeatures.add(rin).bodies.item(0)
    body.name = name
    return body


def cut(comp, body, curve_fn, z0, h):
    f = comp.features
    k = comp.sketches.add(comp.xYConstructionPlane)
    curve_fn(k)
    prof = adsk.core.ObjectCollection.create()
    for i in range(k.profiles.count):
        prof.add(k.profiles.item(i))
    ext = f.extrudeFeatures.createInput(prof, adsk.fusion.FeatureOperations.CutFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(h / 10))
    if z0:
        ext.startExtent = adsk.fusion.OffsetStartDefinition.create(adsk.core.ValueInput.createByReal(z0 / 10))
    ext.participantBodies = [body]
    f.extrudeFeatures.add(ext)


def build(comp):
    """Cache d'écrou en (0, 0), cache factice en (30, 0). Renvoie les deux corps."""
    nut = dome(comp, 0, "Cache ecrou M5")
    cut(comp, nut, lambda k: k.sketchCurves.sketchCircles.addByCenterRadius(p3(0, 0), WASHER_D / 20), 0, WASHER_T)
    rh = NUT_AF / math.sqrt(3)

    def hexagon(k):
        pts = [p3(rh * math.cos(math.radians(60 * j)), rh * math.sin(math.radians(60 * j))) for j in range(6)]
        for j in range(6):
            k.sketchCurves.sketchLines.addByTwoPoints(pts[j], pts[(j + 1) % 6])

    cut(comp, nut, hexagon, 0, WASHER_T + NUT_T)
    cut(comp, nut, lambda k: k.sketchCurves.sketchCircles.addByCenterRadius(p3(0, 0), SCREW_D / 20), 0, SCREW_TOP)

    ox = 30.0
    fake = dome(comp, ox, "Cache factice")
    ro, ri, zb = HOLE_D / 2 - 0.2, HOLE_D / 2 - 0.2 - PEG_WALL, -(SPOKE_T + 0.3)  # crochet 0,3 mm sous la branche
    revolve(comp, ox, [(ri, 0.5), (ro, 0.5), (ro, zb), (ro + BARB, zb), (ro + BARB, zb - 0.7), (ro, zb - 1.4), (ri, zb - 1.4)],
            adsk.fusion.FeatureOperations.JoinFeatureOperation, fake)

    def slots(k):  # 2 fentes en croix : 4 doigts souples
        for a in (0, 90):
            u, v = math.cos(math.radians(a)), math.sin(math.radians(a))
            pts = [p3(ox + sx * 10 * u - sy * 0.8 * v, sx * 10 * v + sy * 0.8 * u) for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
            for j in range(4):
                k.sketchCurves.sketchLines.addByTwoPoints(pts[j], pts[(j + 1) % 4])

    cut(comp, fake, slots, zb - 2, -zb + 2)
    return nut, fake


def make(app):
    """Document dédié (le support a le sien) : refait à chaque lancement."""
    for d in [app.documents.item(i) for i in range(app.documents.count)]:
        if d.name.startswith(DOC):
            d.close(False)
    doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    doc.name = DOC
    design = adsk.fusion.Design.cast(app.activeProduct)
    design.designType = adsk.fusion.DesignTypes.ParametricDesignType
    return design, build(design.rootComponent)


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        _, (nut, fake) = make(app)
        ui.messageBox(f"{DOC} : cache d'écrou {nut.physicalProperties.volume:.2f} cm3 (x2), "
                      f"cache factice {fake.physicalProperties.volume:.2f} cm3 (x1).")
    except Exception:
        if ui:
            ui.messageBox("Erreur :\n" + traceback.format_exc())
