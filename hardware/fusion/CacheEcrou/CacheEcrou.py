"""Fusion 360 : cache d'écrou M5 pour les branches du volant (côté conducteur). Source de hardware/cache_ecrou.stl.

Dôme rond, base plate posée sur la face de la branche, emboîté serré sur l'écrou (empreinte hexagonale).
Une seule pièce convient avec ou sans rondelle large Ø 20 : sans rondelle, le bas de l'écrou est dans le
lamage de la rondelle et l'hexagone le tient plus haut. Le bout de la vis passe dans le trou central.
Imprimé base sur le plateau, sans support (plafonds plats en pont courts). À imprimer en 2 exemplaires.

Lancer : Utilitaires > Compléments > Scripts > "+" > dossier CacheEcrou > Exécuter. Crée (ou refait) le
document "Cache ecrou M5".
"""
import math
import traceback

import adsk.core
import adsk.fusion

DOC = "Cache ecrou M5"
R = 12.0           # rayon de la base (couvre la rondelle Ø 20,6 avec 1,7 mm de paroi)
H = 10.0           # hauteur totale du dôme
H_SKIRT = 1.5      # bord droit au pied du dôme avant l'arrondi
WASHER_D = 20.6    # lamage : rondelle large Ø 20 (+ jeu)
WASHER_T = 1.8     # épaisseur rondelle 1,5 + jeu
NUT_AF = 8.1       # écrou M5 = 8,0 sur plats : 8,1 = serré une fois imprimé (8,0 si trop lâche, 8,2 si trop dur)
NUT_T = 4.2        # écrou M5 = 4 mm
SCREW_D = 5.6      # bout de la vis M5 qui dépasse de l'écrou
SCREW_TOP = 8.0    # 2 mm de plastique au sommet

# Arc du dôme : centre sur l'axe, passe par (R, H_SKIRT) et (0, H)
ZC = (H * H - H_SKIRT * H_SKIRT - R * R) / (2 * (H - H_SKIRT))
RA = H - ZC


def dome_x(z):
    """Rayon extérieur du dôme à la hauteur z (mm)."""
    return R if z <= H_SKIRT else math.sqrt(max(0.0, RA * RA - (z - ZC) ** 2))


# Parois minimales (mm) : au-dessus de l'écrou, du bout de vis et autour du lamage
assert dome_x(WASHER_T + NUT_T) - NUT_AF / math.sqrt(3) > 3
assert dome_x(SCREW_TOP) - SCREW_D / 2 > 3 and H - SCREW_TOP >= 2
assert R - WASHER_D / 2 >= 1.6


def p3(x, y, z=0.0):
    return adsk.core.Point3D.create(x / 10, y / 10, z / 10)


def build(comp):
    f = comp.features
    # Profil du dôme dans le plan XZ, révolution autour de Z
    sk = comp.sketches.add(comp.xZConstructionPlane)
    s = lambda x, z: sk.modelToSketchSpace(p3(x, 0, z))
    L = sk.sketchCurves.sketchLines
    a = L.addByTwoPoints(s(0, 0), s(R, 0))
    b = L.addByTwoPoints(a.endSketchPoint, s(R, H_SKIRT))
    zm = ZC + RA * math.sin(math.radians(60))  # point intermédiaire de l'arc
    arc = sk.sketchCurves.sketchArcs.addByThreePoints(b.endSketchPoint, s(math.sqrt(RA * RA - (zm - ZC) ** 2), zm), s(0, H))
    # Un arc de Fusion va toujours dans le sens trigo : son extrémité sur l'axe peut être le début ou la fin
    top = min((arc.startSketchPoint, arc.endSketchPoint), key=lambda p: abs(p.geometry.x))
    axis = L.addByTwoPoints(top, a.startSketchPoint)
    rin = f.revolveFeatures.createInput(sk.profiles.item(0), axis, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
    rin.setAngleExtent(False, adsk.core.ValueInput.createByString("360 deg"))
    body = f.revolveFeatures.add(rin).bodies.item(0)
    body.name = DOC

    def cut(curve_fn, h):
        k = comp.sketches.add(comp.xYConstructionPlane)
        curve_fn(k)
        prof = adsk.core.ObjectCollection.create()
        for i in range(k.profiles.count):
            prof.add(k.profiles.item(i))
        ext = f.extrudeFeatures.createInput(prof, adsk.fusion.FeatureOperations.CutFeatureOperation)
        ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(h / 10))
        ext.participantBodies = [body]
        f.extrudeFeatures.add(ext)

    cut(lambda k: k.sketchCurves.sketchCircles.addByCenterRadius(p3(0, 0), WASHER_D / 20), WASHER_T)
    rh = NUT_AF / math.sqrt(3)

    def hexagon(k):
        pts = [p3(rh * math.cos(math.radians(60 * j)), rh * math.sin(math.radians(60 * j))) for j in range(6)]
        for j in range(6):
            k.sketchCurves.sketchLines.addByTwoPoints(pts[j], pts[(j + 1) % 6])

    cut(hexagon, WASHER_T + NUT_T)
    cut(lambda k: k.sketchCurves.sketchCircles.addByCenterRadius(p3(0, 0), SCREW_D / 20), SCREW_TOP)
    return body


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
        _, body = make(app)
        ui.messageBox(f"{DOC} créé : {body.physicalProperties.volume:.2f} cm3. À imprimer en 2 exemplaires.")
    except Exception:
        if ui:
            ui.messageBox("Erreur :\n" + traceback.format_exc())
