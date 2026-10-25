"""Erzeugt STL und STEP aus der Quaderbeschreibung in geometry.py.

Keine Abhaengigkeiten, nur die Standardbibliothek.

    python cad/tools/build_mesh.py

Schreibt nach cad/export/. Die Koerper sind achsparallel zusammengesetzt; der
Schnitt wird exakt gerechnet (Gitter aus allen vorkommenden Koordinaten), nicht
angenaehert. Runde Formen - Gehaeuseecken, Sensoroeffnung, Lichtfenster, Loch
der Oese - sind hier Quadrate; die runde Fassung macht `cad/housing.py` in
Fusion 360.

Am Ende laeuft eine Selbstpruefung: jede Kante muss genau zweimal und
gegenlaeufig vorkommen, sonst ist das Netz nicht dicht und die Datei wertlos.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import geometry  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir, "export")

# Reihenfolge der sechs Nachbarn und die zugehoerige aeussere Normale
NEIGHBOURS = (
    (-1, 0, 0), (1, 0, 0),
    (0, -1, 0), (0, 1, 0),
    (0, 0, -1), (0, 0, 1),
)


def _axis_values(ops, index):
    vals = set()
    for _sign, box in ops:
        vals.add(box[index * 2])
        vals.add(box[index * 2 + 1])
    return sorted(vals)


def _inside(ops, px, py, pz):
    state = False
    for sign, (x0, x1, y0, y1, z0, z1) in ops:
        if x0 < px < x1 and y0 < py < y1 and z0 < pz < z1:
            state = (sign == "+")
    return state


def quads_from_ops(ops):
    """Exakter Schnitt der Quader. Liefert Vierecke mit aeusserer Normale."""
    xs = _axis_values(ops, 0)
    ys = _axis_values(ops, 1)
    zs = _axis_values(ops, 2)
    nx, ny, nz = len(xs) - 1, len(ys) - 1, len(zs) - 1
    if nx < 1 or ny < 1 or nz < 1:
        raise ValueError("Koerper hat kein Volumen")

    solid = bytearray(nx * ny * nz)

    def idx(i, j, k):
        return (i * ny + j) * nz + k

    for i in range(nx):
        cx = (xs[i] + xs[i + 1]) / 2.0
        for j in range(ny):
            cy = (ys[j] + ys[j + 1]) / 2.0
            for k in range(nz):
                cz = (zs[k] + zs[k + 1]) / 2.0
                if _inside(ops, cx, cy, cz):
                    solid[idx(i, j, k)] = 1

    quads = []
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                if not solid[idx(i, j, k)]:
                    continue
                x0, x1 = xs[i], xs[i + 1]
                y0, y1 = ys[j], ys[j + 1]
                z0, z1 = zs[k], zs[k + 1]
                for di, dj, dk in NEIGHBOURS:
                    ii, jj, kk = i + di, j + dj, k + dk
                    if 0 <= ii < nx and 0 <= jj < ny and 0 <= kk < nz and solid[idx(ii, jj, kk)]:
                        continue
                    # Eckpunkte gegen den Uhrzeigersinn, von aussen gesehen
                    if di == -1:
                        ring = [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)]
                    elif di == 1:
                        ring = [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]
                    elif dj == -1:
                        ring = [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]
                    elif dj == 1:
                        ring = [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]
                    elif dk == -1:
                        ring = [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)]
                    else:
                        ring = [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
                    quads.append(((float(di), float(dj), float(dk)), ring))
    return quads


def check_watertight(quads):
    """Jede Kante genau zweimal, und zwar gegenlaeufig. Sonst Fehler."""
    seen = {}
    for _n, ring in quads:
        for a in range(4):
            e = (ring[a], ring[(a + 1) % 4])
            if e in seen:
                raise ValueError("Kante doppelt in gleicher Richtung: %s" % (e,))
            seen[e] = True
    for (a, b) in list(seen):
        if (b, a) not in seen:
            raise ValueError("Kante ohne Gegenstueck: %s -> %s" % (a, b))
    return len(seen) // 2


def volume(quads):
    """Volumen ueber den Satz von Gauss - muss positiv sein."""
    total = 0.0
    for _n, ring in quads:
        for tri in ((ring[0], ring[1], ring[2]), (ring[0], ring[2], ring[3])):
            (ax, ay, az), (bx, by, bz), (cx, cy, cz) = tri
            total += (ax * (by * cz - bz * cy)
                      - ay * (bx * cz - bz * cx)
                      + az * (bx * cy - by * cx)) / 6.0
    return total


def write_stl(path, name, quads):
    tris = []
    for normal, ring in quads:
        tris.append((normal, (ring[0], ring[1], ring[2])))
        tris.append((normal, (ring[0], ring[2], ring[3])))
    with open(path, "wb") as fh:
        header = ("opensecurekey %s - erzeugt von cad/tools/build_mesh.py" % name)
        fh.write(header.encode("ascii", "replace").ljust(80, b" ")[:80])
        fh.write(struct.pack("<I", len(tris)))
        for normal, tri in tris:
            fh.write(struct.pack("<3f", *normal))
            for v in tri:
                fh.write(struct.pack("<3f", *v))
            fh.write(struct.pack("<H", 0))
    return len(tris)


class StepWriter:
    """Schreibt die Vierecke als flaechigen B-Rep (AP214, planare Flaechen)."""

    def __init__(self, name):
        self.name = name
        self.lines = []
        self.n = 0
        self.points = {}
        self.verts = {}
        self.dirs = {}
        self.edges = {}

    def _add(self, text):
        self.n += 1
        self.lines.append("#%d=%s;" % (self.n, text))
        return self.n

    @staticmethod
    def _num(v):
        return ("%.6f" % v).rstrip("0").rstrip(".") or "0."

    def _fmt(self, v):
        s = self._num(v)
        return s if "." in s else s + "."

    def point(self, p):
        if p not in self.points:
            self.points[p] = self._add("CARTESIAN_POINT('',(%s,%s,%s))"
                                       % tuple(self._fmt(c) for c in p))
        return self.points[p]

    def vertex(self, p):
        if p not in self.verts:
            self.verts[p] = self._add("VERTEX_POINT('',#%d)" % self.point(p))
        return self.verts[p]

    def direction(self, d):
        if d not in self.dirs:
            self.dirs[d] = self._add("DIRECTION('',(%s,%s,%s))"
                                     % tuple(self._fmt(c) for c in d))
        return self.dirs[d]

    def edge(self, a, b):
        key = (a, b) if a <= b else (b, a)
        if key not in self.edges:
            p0, p1 = key
            vec = tuple(p1[i] - p0[i] for i in range(3))
            length = sum(c * c for c in vec) ** 0.5
            unit = tuple(c / length for c in vec)
            v = self._add("VECTOR('',#%d,%s)" % (self.direction(unit), self._fmt(length)))
            line = self._add("LINE('',#%d,#%d)" % (self.point(p0), v))
            self.edges[key] = self._add("EDGE_CURVE('',#%d,#%d,#%d,.T.)"
                                        % (self.vertex(p0), self.vertex(p1), line))
        return self.edges[key], key[0] == a

    def face(self, normal, ring):
        oriented = []
        for i in range(4):
            a, b = ring[i], ring[(i + 1) % 4]
            eid, same = self.edge(a, b)
            oriented.append(self._add("ORIENTED_EDGE('',*,*,#%d,%s)"
                                      % (eid, ".T." if same else ".F.")))
        loop = self._add("EDGE_LOOP('',(%s))" % ",".join("#%d" % o for o in oriented))
        bound = self._add("FACE_OUTER_BOUND('',#%d,.T.)" % loop)
        # Bezugsrichtung in der Ebene: die erste Kante des Rings
        ref = tuple(ring[1][i] - ring[0][i] for i in range(3))
        rlen = sum(c * c for c in ref) ** 0.5
        ref = tuple(c / rlen for c in ref)
        plc = self._add("AXIS2_PLACEMENT_3D('',#%d,#%d,#%d)"
                        % (self.point(ring[0]), self.direction(normal), self.direction(ref)))
        plane = self._add("PLANE('',#%d)" % plc)
        return self._add("ADVANCED_FACE('',(#%d),#%d,.T.)" % (bound, plane))

    def build(self, quads):
        faces = [self.face(n, r) for n, r in quads]
        shell = self._add("CLOSED_SHELL('',(%s))" % ",".join("#%d" % f for f in faces))
        origin = self.point((0.0, 0.0, 0.0))
        axis = self._add("AXIS2_PLACEMENT_3D('',#%d,#%d,#%d)"
                         % (origin, self.direction((0.0, 0.0, 1.0)),
                            self.direction((1.0, 0.0, 0.0))))
        brep = self._add("MANIFOLD_SOLID_BREP('%s',#%d)" % (self.name, shell))

        mm = self._add("(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.MILLI.,.METRE.))")
        rad = self._add("(NAMED_UNIT(*)PLANE_ANGLE_UNIT()SI_UNIT($,.RADIAN.))")
        sr = self._add("(NAMED_UNIT(*)SOLID_ANGLE_UNIT()SI_UNIT($,.STERADIAN.))")
        tol = self._add("UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(0.001),#%d,"
                        "'closure','Toleranz')" % mm)
        ctx = self._add("(GEOMETRIC_REPRESENTATION_CONTEXT(3)"
                        "GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#%d))"
                        "GLOBAL_UNIT_ASSIGNED_CONTEXT((#%d,#%d,#%d))"
                        "REPRESENTATION_CONTEXT('',''))" % (tol, mm, rad, sr))
        rep = self._add("ADVANCED_BREP_SHAPE_REPRESENTATION('%s',(#%d,#%d),#%d)"
                        % (self.name, axis, brep, ctx))

        appctx = self._add("APPLICATION_CONTEXT("
                           "'automotive design')")
        self._add("APPLICATION_PROTOCOL_DEFINITION('international standard',"
                  "'automotive_design',2000,#%d)" % appctx)
        pdc = self._add("PRODUCT_DEFINITION_CONTEXT('part definition',#%d,'design')" % appctx)
        pc = self._add("PRODUCT_CONTEXT('',#%d,'mechanical')" % appctx)
        prod = self._add("PRODUCT('%s','%s','',(#%d))" % (self.name, self.name, pc))
        pdf = self._add("PRODUCT_DEFINITION_FORMATION('1','',#%d)" % prod)
        pd = self._add("PRODUCT_DEFINITION('design','',#%d,#%d)" % (pdf, pdc))
        pds = self._add("PRODUCT_DEFINITION_SHAPE('','',#%d)" % pd)
        self._add("SHAPE_DEFINITION_REPRESENTATION(#%d,#%d)" % (pds, rep))
        mech = self._add("MECHANICAL_CONTEXT('',#%d,'mechanical')" % appctx)
        self.lines[mech - 1] = "#%d=MECHANICAL_CONTEXT('',#%d,'mechanical');" % (mech, appctx)
        return len(faces)

    def text(self):
        head = [
            "ISO-10303-21;",
            "HEADER;",
            "FILE_DESCRIPTION(('opensecurekey Gehaeuse'),'2;1');",
            "FILE_NAME('%s.step','',('Sorglos Thomas Weirich'),(''),"
            "'cad/tools/build_mesh.py','',''); " % self.name,
            "FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));",
            "ENDSEC;",
            "DATA;",
        ]
        return "\n".join(head + self.lines + ["ENDSEC;", "END-ISO-10303-21;", ""])

    def check(self):
        """Jede benutzte Nummer muss auch vergeben sein."""
        defined = set(range(1, self.n + 1))
        import re
        used = set()
        for line in self.lines:
            body = line.split("=", 1)[1]
            used.update(int(m) for m in re.findall(r"#(\d+)", body))
        missing = sorted(used - defined)
        if missing:
            raise ValueError("STEP verweist auf nicht vergebene Nummern: %s" % missing[:10])
        return len(used)


def check_openings(p, d):
    """Stichproben im Koerper: ist jede Oeffnung wirklich offen?

    Der Anlass steht in cad/gehaeuse.md, Befund 4: der Schnappfalz der unteren
    Schale stand vor der Tasteroeffnung und vor der USB-Oeffnung. Das zeigt
    keine Dickenrechnung und kein Dichtheitstest - nur eine Probe an der
    Stelle, an der etwas frei sein muss.
    """
    top = geometry.shell_top(p, d)
    bot = geometry.shell_bottom(p, d)
    bx = p["btn_cx"]
    sx = p["sw2_cx"]
    lip_mid = p["lip_h"] / 2.0          # in der Schnappstufe
    wall_mid = (d["ot_y0"] + d["pk_y0"] - p["lip_t"]) / 2.0   # aeussere Wandhaelfte
    front_mid = (d["ot_x0"] + d["pk_x0"] - p["lip_t"]) / 2.0
    probes = [
        ("Falz vor dem SW1-Schlitz", bot, (bx, wall_mid, lip_mid), False),
        ("Falz neben dem SW1-Schlitz", bot, (bx + 15.0, wall_mid, lip_mid), True),
        ("Falz vor der USB-Oeffnung", bot, (front_mid, p["usb_cy"], lip_mid), False),
        ("Falz neben der USB-Oeffnung", bot, (front_mid, 3.0, lip_mid), True),
        ("Kragen ueber SW1", top, (bx, d["pk_y0"] + 0.5, p["btn_relief_z"] - 0.5), False),
        ("Kragen ueber SW2", top, (sx, d["pk_y1"] - 0.5, p["btn_relief_z"] - 0.5), False),
        ("Kragen neben den Tastern", top, (bx + 15.0, d["pk_y0"] + 0.5, 0.8), True),
        # Gegenprobe zum Umzug: an der alten SW2-Stelle muss der Kragen wieder
        # voll stehen, sonst ist die alte Freistellung stehen geblieben
        ("Kragen an der alten SW2-Stelle", top,
         (bx, d["pk_y1"] - 0.5, p["btn_relief_z"] - 0.5), True),
        ("SW1-Wand ist durchbrochen", top, (bx, wall_mid, p["btn_z"]), False),
        ("SW2-Wand bleibt stehen", top,
         (sx, d["ot_y1"] - p["lip_t"] / 2.0, d["z_cav_top"] - 0.2), True),
        # Der Falz der unteren Schale darf auch an der neuen SW2-Stelle nicht
        # im Weg sein - dort steht er aussen, die Blindtasche liegt innen
        ("Falz an der SW2-Blindtasche", bot,
         (sx, d["ot_y1"] - p["lip_t"] / 2.0, lip_mid), True),
        ("Dach ueber dem Hohlraum", top,
         (bx, p["pcb_wid"] / 2.0 + 5.0, d["z_roof"] - 0.2), True),
    ]
    bad = []
    for name, ops, (x, y, z), want in probes:
        if _inside(ops, x, y, z) != want:
            bad.append("%s: erwartet %s" % (name, "voll" if want else "frei"))
    if bad:
        raise ValueError("Probe im Koerper fehlgeschlagen - " + "; ".join(bad))
    return len(probes)


def main():
    p = geometry.values()
    d = geometry.derive(p)
    if not os.path.isdir(OUT):
        os.makedirs(OUT)

    print("Dicke gesamt      : %.2f mm" % d["total_th"])
    print("Aussen ohne Oese  : %.2f x %.2f mm" % (d["outer_len"], d["outer_wid"]))
    print("Aussen mit Oese   : %.2f mm lang" % d["overall_len"])
    print("")

    for name, fn in geometry.BODIES:
        ops = fn(p, d)
        quads = quads_from_ops(ops)
        edges = check_watertight(quads)
        vol = volume(quads)
        if vol <= 0.0:
            raise ValueError("%s: Volumen %.3f - Normalen zeigen nach innen" % (name, vol))
        tris = write_stl(os.path.join(OUT, name + ".stl"), name, quads)
        w = StepWriter(name)
        faces = w.build(quads)
        refs = w.check()
        with open(os.path.join(OUT, name + ".step"), "w", encoding="ascii",
                  newline="\n") as fh:
            fh.write(w.text())
        print("%-13s  %5d Flaechen  %5d Dreiecke  %5d Kanten  %8.1f mm3  "
              "STEP %d Zeilen, %d Verweise geprueft"
              % (name, faces, tris, edges, vol, w.n, refs))

    print("\n%d Proben an den Oeffnungen bestanden" % check_openings(p, d))
    print("check: ok")


if __name__ == "__main__":
    main()
