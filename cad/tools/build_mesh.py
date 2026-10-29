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


def probe_list(p, d):
    """Die Proben im Koerper: (Name, Koerper, Punkt, soll_voll).

    Eine Dickenrechnung zeigt nicht, ob eine Oeffnung wirklich offen ist, und
    ein Dichtheitstest auch nicht. Dafuer sind diese Proben da - und zu jeder
    Stelle, die FREI sein muss, steht eine daneben, die VOLL sein muss.
    """
    top = geometry.shell_top(p, d)
    bot = geometry.shell_bottom(p, d)
    # In der Hoehe des Klebefalzes gehoert die AEUSSERE Wandhaelfte der
    # unteren Schale und die INNERE der oberen. Darum zwei Abtastpunkte.
    wall_x = (d["ot_x0"] + d["pk_x0"]) / 2.0                  # Wandmitte, untere Schale
    wall_x_top = (d["ot_x0"] + p["join_lap_t"] + d["pk_x0"]) / 2.0   # innere Haelfte
    front_y_top = (d["ot_y0"] + p["join_lap_t"] + d["pk_y0"]) / 2.0
    btn_z = d["z_pcb_top"] + p["btn_z"]
    pot_z = d["z_pot_top"] - 0.2                    # knapp unter der Vergussoberflaeche
    bot_z = d["z_pcb_bot"] - 0.3                    # im unteren Vergussraum
    roof_z = d["z_roof"] - 0.2
    sw1, sw2 = p["sw1_cy"], p["sw2_cy"]
    return [
        # --- Taster
        ("SW1: Wand ist durchbrochen", top, (wall_x_top, sw1, btn_z), False),
        ("SW1: Wand steht daneben", top, (wall_x_top, sw1 + 10.0, btn_z), True),
        ("SW2: Blindtasche ist frei", top,
         (d["pk_x1"] + p["sw2_pocket_d"] / 2.0, sw2, btn_z), False),
        ("SW2: Wand bleibt stehen", top,
         (d["pk_x1"] + p["sw2_pocket_d"] + p["wall_t"] / 4.0, sw2,
          d["z_pcb_top"] + p["join_lap"] + 0.2), True),
        ("SW2: Falz der unteren Schale steht davor", bot,
         (d["pk_x1"] + p["sw2_pocket_d"] + p["wall_t"] / 4.0, sw2,
          d["z_pcb_top"] + p["join_lap"] / 2.0), True),
        ("SW2: Damm steht", top, (p["sw2_island_x0"] - p["pot_dam_t"] / 2.0, sw2, pot_z), True),
        ("SW2: Insel ist trocken", top, (p["sw2_island_x0"] + 1.5, sw2, pot_z), False),
        # --- Anzeige
        ("Anzeige: Damm davor steht", top,
         (p["oled_cx"], p["oled_glass_y0"] - p["pot_dam_t_min"] / 2.0, pot_z), True),
        ("Anzeige: Insel ist trocken", top, (p["oled_cx"], d["win_cy"], pot_z), False),
        ("Anzeige: Fenster ist offen", top, (p["oled_cx"], d["win_cy"], roof_z), False),
        ("Anzeige: Dach steht daneben", top, (p["oled_cx"], 18.0, roof_z), True),
        # --- Verguss oben
        ("Verguss oben: Raum ist frei", top, (p["oled_cx"], 16.0, pot_z), False),
        ("Einguss ist offen", top, (d["ports"][0][0], d["ports"][0][1], roof_z), False),
        ("Dach neben dem Einguss steht", top, (8.0, 10.0, roof_z), True),
        # --- Stecker
        ("Kragen: Oeffnung ist frei", top, (d["usb_cx"], front_y_top, btn_z), False),
        ("Frontwand steht daneben", top, (3.0, front_y_top, btn_z), True),
        ("Stecker: Insel ist trocken", bot, (d["usb_cx"], 2.0, bot_z), False),
        ("Stecker: Damm dahinter steht", bot,
         (d["usb_cx"], d["usb_y1"] + p["pot_dam_t"] / 2.0, bot_z), True),
        # --- microSD
        ("microSD: Schlitz ist offen", bot, (wall_x, (p["sd_slot_y0"] + p["sd_slot_y1"]) / 2.0, 0.0), False),
        ("microSD: Wand steht daneben", bot, (wall_x, 50.0, 0.0), True),
        ("microSD: Schacht ist trocken", bot, (7.0, 33.9, bot_z), False),
        ("microSD: Damm steht", bot,
         (p["sd_body_x1"] + p["pot_island_fit"] + p["pot_dam_t"] / 2.0, 33.9, bot_z), True),
        # --- Verguss unten, Auflage, Klebefalz
        ("Verguss unten: Raum ist frei", bot, (20.0, 15.0, bot_z), False),
        ("Boden steht darunter", bot, (20.0, 15.0, d["z_floor"] + 0.2), True),
        ("Platinentasche ist frei", bot, (p["oled_cx"], 30.0, 0.0), False),
        ("Auflage traegt die Platine", bot, (d["pk_x0"] + p["seal_w"] / 2.0, 50.0, bot_z), True),
        ("Klebefalz: aussere Haelfte gehoert unten", bot,
         (d["ot_x0"] + p["join_lap_t"] / 2.0, 50.0, d["z_pcb_top"] + p["join_lap"] / 2.0), True),
        ("Klebefalz: innere Haelfte gehoert oben", bot,
         (d["pk_x0"] - p["join_lap_t"] / 2.0, 50.0, d["z_pcb_top"] + p["join_lap"] / 2.0), False),
        ("Klebefalz: innere Haelfte steht oben", top,
         (d["pk_x0"] - p["join_lap_t"] / 2.0, 50.0, d["z_pcb_top"] + p["join_lap"] / 2.0), True),
    ]


def check_openings(p, d):
    """Fuehrt die Proben aus. Liefert (Anzahl, Anzahl der 'muss voll')."""
    probes = probe_list(p, d)
    bad = []
    for name, ops, (x, y, z), want in probes:
        if _inside(ops, x, y, z) != want:
            bad.append("%s: erwartet %s" % (name, "voll" if want else "frei"))
    if bad:
        raise ValueError("Probe im Koerper fehlgeschlagen - " + "; ".join(bad))
    return len(probes), sum(1 for t in probes if t[3])


def main():
    p = geometry.values()
    d = geometry.derive(p)
    if not os.path.isdir(OUT):
        os.makedirs(OUT)

    print("Gehaeusedicke     : %.2f mm (Aussenflaeche unten bis oben)" % d["total_th"])
    print("Aussen ohne Oese  : %.2f mm lang x %.2f mm breit" % (d["outer_len"], d["outer_wid"]))
    print("Aussen mit Oese   : %.2f mm lang" % d["overall_len"])
    print("Steckerversatz    : %+.3f mm (Steckermitte gegen Gehaeusemitte)" % d["plug_offset"])
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

    anzahl, voll = check_openings(p, d)
    print("\n%d Proben im Koerper bestanden - %d davon muessen VOLL sein, %d FREI"
          % (anzahl, voll, anzahl - voll))
    print("check: ok")


if __name__ == "__main__":
    main()
