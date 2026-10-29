"""opensecurekey - Gehaeuse in Fusion 360 bauen.

Fusion-Skript. Es legt die Masse als Benutzerparameter an, baut die zwei
Schalen und schreibt STEP und STL nach `cad/export/`.

So wird es benutzt:

1. Diesen Ordner `cad` dorthin kopieren oder verknuepfen, wo Fusion Skripte
   sucht, oder in Fusion ueber Utilities -> ADD-INS -> Scripts -> "+" diese
   Datei hinzufuegen.
2. Eine **leere** neue Konstruktion oeffnen.
3. Skript ausfuehren.

Die Quelle aller Masse ist `cad/parameters.csv`. Im Koerper steht keine Zahl,
die nicht von dort kommt. Aendert sich die Platinengroesse, wird **nur** die
CSV geaendert und das Skript neu ausgefuehrt - es baut von Null auf, es gibt
also keinen alten Stand, der nachgezogen werden muesste.

Zwei Teile, nicht drei: der Sensorsitz der Stufe 2 gehoert nicht in diesen
Lauf (siehe masse.md Abschnitt 6).

Grenze, offen gesagt: die Benutzerparameter werden angelegt und beschriftet,
aber die Skizzen sind nicht an sie bemasst. Ein Parameter, der in Fusion von
Hand verstellt wird, aendert den Koerper also nicht - dafuer die CSV aendern
und neu laufen lassen.

Ohne Fusion laeuft `cad/tools/build_mesh.py`; es baut dieselben Koerper aus
derselben Beschreibung, nur mit Quadraten statt Rundungen.
"""

import os
import sys
import traceback

import adsk.core
import adsk.fusion

HERE = os.path.dirname(os.path.abspath(__file__))
if os.path.join(HERE, "tools") not in sys.path:
    sys.path.insert(0, os.path.join(HERE, "tools"))

import budget  # noqa: E402
import geometry  # noqa: E402

EXPORT = os.path.join(HERE, "export")

R_OUTER = "outer_corner_r"


class Builder:
    """Baut einen Koerper aus Quadern und Zylindern, in der Reihenfolge der Liste."""

    def __init__(self, design, name):
        self.design = design
        root = design.rootComponent
        occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        occ.component.name = name
        self.comp = occ.component
        self.occ = occ
        self.body = None

    # -- Grundkoerper ----------------------------------------------------
    def _extrude(self, sketch, z0, height):
        prof = sketch.profiles.item(0)
        ext = self.comp.features.extrudeFeatures
        inp = ext.createInput(prof, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
        inp.setDistanceExtent(False, adsk.core.ValueInput.createByReal(height / 10.0))
        if abs(z0) > 1e-9:
            inp.startExtent = adsk.fusion.OffsetStartDefinition.create(
                adsk.core.ValueInput.createByReal(z0 / 10.0))
        return ext.add(inp).bodies.item(0)

    def _rect_body(self, x0, x1, y0, y1, z0, z1):
        sk = self.comp.sketches.add(self.comp.xYConstructionPlane)
        sk.isComputeDeferred = True
        sk.sketchCurves.sketchLines.addTwoPointRectangle(
            adsk.core.Point3D.create(x0 / 10.0, y0 / 10.0, 0),
            adsk.core.Point3D.create(x1 / 10.0, y1 / 10.0, 0))
        sk.isComputeDeferred = False
        return self._extrude(sk, z0, z1 - z0)

    def _circle_body(self, cx, cy, dia, z0, z1):
        sk = self.comp.sketches.add(self.comp.xYConstructionPlane)
        sk.isComputeDeferred = True
        sk.sketchCurves.sketchCircles.addByCenterRadius(
            adsk.core.Point3D.create(cx / 10.0, cy / 10.0, 0), dia / 20.0)
        sk.isComputeDeferred = False
        return self._extrude(sk, z0, z1 - z0)

    # -- Verknuepfen -----------------------------------------------------
    def _combine(self, tool, operation):
        if self.body is None:
            if operation != adsk.fusion.FeatureOperations.JoinFeatureOperation:
                raise ValueError("erster Schritt muss ein Hinzufuegen sein")
            self.body = tool
            return
        tools = adsk.core.ObjectCollection.create()
        tools.add(tool)
        comb = self.comp.features.combineFeatures
        inp = comb.createInput(self.body, tools)
        inp.operation = operation
        inp.isKeepToolBodies = False
        self.body = comb.add(inp).bodies.item(0)

    def add_rect(self, box):
        self._combine(self._rect_body(*box), adsk.fusion.FeatureOperations.JoinFeatureOperation)

    def cut_rect(self, box):
        self._combine(self._rect_body(*box), adsk.fusion.FeatureOperations.CutFeatureOperation)

    def add_circle(self, cx, cy, dia, z0, z1):
        self._combine(self._circle_body(cx, cy, dia, z0, z1),
                      adsk.fusion.FeatureOperations.JoinFeatureOperation)

    def cut_circle(self, cx, cy, dia, z0, z1):
        self._combine(self._circle_body(cx, cy, dia, z0, z1),
                      adsk.fusion.FeatureOperations.CutFeatureOperation)

    # -- Rundungen -------------------------------------------------------
    def fillet_vertical_at(self, corners, radius, tol=0.05):
        """Rundet die senkrechten Kanten, die an einer der genannten Ecken liegen."""
        edges = adsk.core.ObjectCollection.create()
        for edge in self.body.edges:
            geo = edge.geometry
            if geo.objectType != adsk.core.Line3D.classType():
                continue
            a, b = geo.startPoint, geo.endPoint
            if abs(a.x - b.x) > 1e-6 or abs(a.y - b.y) > 1e-6:
                continue  # nicht senkrecht
            for cx, cy in corners:
                if abs(a.x * 10.0 - cx) < tol and abs(a.y * 10.0 - cy) < tol:
                    edges.add(edge)
                    break
        if edges.count == 0:
            return 0
        fil = self.comp.features.filletFeatures
        inp = fil.createInput()
        inp.isRollingBallCorner = True
        inp.edgeSetInputs.addConstantRadiusEdgeSet(
            edges, adsk.core.ValueInput.createByReal(radius / 10.0), True)
        fil.add(inp)
        return edges.count


def make_parameters(design, params):
    """Legt die Masse als Benutzerparameter an - sie dokumentieren den Stand."""
    up = design.userParameters
    created = 0
    for name, (value, comment) in params.items():
        if up.itemByName(name) is not None:
            up.itemByName(name).expression = "%g mm" % value
            continue
        up.add(name, adsk.core.ValueInput.createByString("%g mm" % value),
               "mm", comment)
        created += 1
    return created


def _outer_corners(d):
    """Die vier Ecken, die aussen gerundet werden: vorn am Stecker, hinten an der Oese."""
    return [(d["ot_x0"], d["ot_y0"]), (d["ot_x1"], d["ot_y0"]),
            (d["eye_x0"], d["eye_y1"]), (d["eye_x1"], d["eye_y1"])]


def build_shell_top(design, p, d):
    b = Builder(design, "shell-top")
    for sign, box in geometry.shell_top(p, d, preview=False):
        if sign == "+":
            b.add_rect(box)
        else:
            b.cut_rect(box)
    # die runden Formen, die die Vorschau als Quadrate zeigt
    b.cut_circle(p["led_cx"], d["led_cy"], p["led_win_d"], d["z_pot_top"] - 0.1,
                 d["z_roof"] + 0.1)
    for cx, cy in d["ports"]:
        b.cut_circle(cx, cy, p["pot_port_d"], d["z_pot_top"] - 0.1, d["z_roof"] + 0.1)
    for cx, cy in d["vents"]:
        b.cut_circle(cx, cy, p["pot_vent_d"], d["z_pot_top"] - 0.1, d["z_roof"] + 0.1)
    b.cut_circle(d["eye_cx"], d["eye_cy"], p["eye_hole_d"],
                 d["z_pcb_top"] - 0.1, d["z_roof"] + 0.1)
    b.fillet_vertical_at(_outer_corners(d), p[R_OUTER])
    return b


def build_shell_bottom(design, p, d):
    b = Builder(design, "shell-bottom")
    for sign, box in geometry.shell_bottom(p, d, preview=False):
        if sign == "+":
            b.add_rect(box)
        else:
            b.cut_rect(box)
    b.cut_circle(d["eye_cx"], d["eye_cy"], p["eye_hole_d"],
                 d["z_floor"] - 0.1, d["z_pcb_top"] + p["join_lap"] + 0.1)
    b.fillet_vertical_at(_outer_corners(d), p[R_OUTER])
    return b


def export_all(design, builders):
    if not os.path.isdir(EXPORT):
        os.makedirs(EXPORT)
    mgr = design.exportManager
    done = []
    for b in builders:
        base = os.path.join(EXPORT, b.comp.name)
        mgr.execute(mgr.createSTEPExportOptions(base + ".step", b.comp))
        stl = mgr.createSTLExportOptions(b.body, base + ".stl")
        stl.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
        stl.isBinaryFormat = True
        mgr.execute(stl)
        done.append(b.comp.name)
    # dazu die ganze Konstruktion als Fusion-Datei
    mgr.execute(mgr.createFusionArchiveExportOptions(
        os.path.join(EXPORT, "housing.f3d")))
    return done


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        design = adsk.fusion.Design.cast(app.activeProduct)
        if design is None:
            ui.messageBox("Es ist keine Fusion-Konstruktion offen.")
            return
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        design.unitsManager.distanceDisplayUnits = \
            adsk.fusion.DistanceUnits.MillimeterDistanceUnits

        table = geometry.read_parameters()
        p = dict((k, v[0]) for k, v in table.items())
        d = geometry.derive(p)
        make_parameters(design, table)

        builders = [build_shell_top(design, p, d),
                    build_shell_bottom(design, p, d)]
        names = export_all(design, builders)

        schlecht = budget.selftest(p)
        waerme = budget.waerme(p, d)

        ui.messageBox(
            "Gehaeuse gebaut.\n\n"
            "Gehaeusedicke: %.2f mm (Aussenflaeche unten bis oben)\n"
            "Aussen ohne Oese: %.2f mm lang x %.2f mm breit\n"
            "Aussen mit Oese: %.2f mm lang\n"
            "Steckerversatz: %+.3f mm gegen die Gehaeusemitte\n"
            "Uebertemperatur bei Spitzenlast: %.1f K\n"
            "Kontrollpunkte der Hoehenrechnung: %s\n\n"
            "Geschrieben nach cad/export/: %s\n(je STEP und STL, dazu housing.f3d)"
            % (d["total_th"], d["outer_len"], d["outer_wid"], d["overall_len"],
               d["plug_offset"], waerme["dt_k"],
               "alle wie erwartet" if not schlecht else "FEHLER: " + "; ".join(schlecht),
               ", ".join(names)))
    except Exception:
        if ui:
            ui.messageBox("Fehlgeschlagen:\n%s" % traceback.format_exc())
        else:
            raise
