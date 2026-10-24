"""opensecurekey - Gehaeuse in Fusion 360 bauen.

Fusion-Skript. Es legt die Maße als Benutzerparameter an, baut die drei Teile
und schreibt STEP und STL nach `cad/export/`.

So wird es benutzt:

1. Diesen Ordner `cad` dorthin kopieren oder verknuepfen, wo Fusion Skripte
   sucht, oder in Fusion ueber Utilities -> ADD-INS -> Scripts -> "+" diese
   Datei hinzufuegen.
2. Eine **leere** neue Konstruktion oeffnen.
3. Skript ausfuehren.

Die Quelle aller Maße ist `cad/parameters.csv`. Im Koerper steht keine Zahl,
die nicht von dort kommt. Aendert sich die Platinengroesse, wird **nur** die
CSV geaendert und das Skript neu ausgefuehrt - es baut von Null auf, es gibt
also keinen alten Stand, der nachgezogen werden muesste.

Grenze, offen gesagt: die Benutzerparameter werden angelegt und beschriftet,
aber die Skizzen sind nicht an sie bemaßt. Ein Parameter, der in Fusion von
Hand verstellt wird, aendert den Koerper also nicht - dafuer die CSV aendern
und neu laufen lassen. Alles andere waere ein viel groesseres Skript, ohne
dass es die Arbeit leichter machen wuerde.
"""

import os
import sys
import traceback

import adsk.core
import adsk.fusion

HERE = os.path.dirname(os.path.abspath(__file__))
if os.path.join(HERE, "tools") not in sys.path:
    sys.path.insert(0, os.path.join(HERE, "tools"))

import geometry  # noqa: E402

EXPORT = os.path.join(HERE, "export")

# Welche Kanten gerundet werden
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
    """Legt die Maße als Benutzerparameter an - sie dokumentieren den Stand."""
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


def build_shell_top(design, p, d):
    b = Builder(design, "shell-top")
    for sign, box in geometry.shell_top(p, d, preview=False):
        if sign == "+":
            b.add_rect(box)
        else:
            b.cut_rect(box)
    # die Vorschau-Quadrate durch echte Rundungen ersetzen
    b.cut_circle(p["led_cx"], p["led_cy"], p["led_win_d"], d["z_cav_top"] - 0.1, d["z_roof"] + 0.1)
    b.cut_circle(d["eye_cx"], d["eye_cy"], p["eye_hole_d"],
                 d["z_pcb_top"] - 0.1, d["z_roof"] + 0.1)
    b.fillet_vertical_at([(d["ot_x0"], d["ot_y0"]), (d["ot_x0"], d["ot_y1"]),
                          (d["eye_x1"], d["eye_y0"]), (d["eye_x1"], d["eye_y1"])],
                         p[R_OUTER])
    return b


def build_shell_bottom(design, p, d):
    b = Builder(design, "shell-bottom")
    for sign, box in geometry.shell_bottom(p, d, preview=False):
        if sign == "+":
            b.add_rect(box)
        else:
            b.cut_rect(box)
    b.cut_circle(p["fp_cx"], p["fp_cy"], p["fp_open_d"],
                 d["z_floor"] - 0.1, d["z_cav_bot"] + 0.1)
    b.cut_circle(d["eye_cx"], d["eye_cy"], p["eye_hole_d"],
                 d["z_floor"] - 0.1, d["z_pcb_top"] + p["lip_h"] + 0.1)
    b.fillet_vertical_at([(d["ot_x0"], d["ot_y0"]), (d["ot_x0"], d["ot_y1"]),
                          (d["eye_x1"], d["eye_y0"]), (d["eye_x1"], d["eye_y1"])],
                         p[R_OUTER])
    return b


def build_sensor_plug(design, p, d):
    b = Builder(design, "sensor-plug")
    z0 = d["z_floor"]  # Vollzylinder, hier gibt es keinen Vorschau-Ersatz
    b.add_circle(p["fp_cx"], p["fp_cy"], p["fp_open_d"] - p["fp_plug_fit"], z0, z0 + p["fp_plug_th"])
    b.add_circle(p["fp_cx"], p["fp_cy"], p["fp_open_d"] + 1.5, z0 - 0.4, z0)
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
                    build_shell_bottom(design, p, d),
                    build_sensor_plug(design, p, d)]
        names = export_all(design, builders)

        ui.messageBox(
            "Gehaeuse gebaut.\n\n"
            "Dicke gesamt: %.2f mm\n"
            "Aussen ohne Oese: %.2f x %.2f mm\n"
            "Aussen mit Oese: %.2f mm lang\n\n"
            "Geschrieben nach cad/export/: %s\n(je STEP und STL, dazu housing.f3d)"
            % (d["total_th"], d["outer_len"], d["outer_wid"], d["overall_len"],
               ", ".join(names)))
    except Exception:
        if ui:
            ui.messageBox("Fehlgeschlagen:\n%s" % traceback.format_exc())
        else:
            raise
