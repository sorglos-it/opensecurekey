"""Geometrie des Gehaeuses aus der Parametertabelle.

UEBERHOLT seit 27.10.2026: dieses Modell hat einen Akkuraum, und der Akku ist
am 25.10. gestrichen worden (docs/plan.md, Abschn. 2). Die Koerper stimmen
damit nicht mehr - auch die Dicke nicht, denn der Zellenraum war das dickste
Element. Neu gezeichnet wird erst, wenn das Platinenmaß steht; bis dahin ist
die Ausgabe dieses Moduls eine Vorschau auf einen alten Entwurf.
Siehe ../masse.md, Kopf.

Keine Abhaengigkeiten. Liefert fuer jeden Koerper eine Liste von Quadern mit
einem Zeichen davor: '+' hinzufuegen, '-' abziehen, in dieser Reihenfolge.
Alle Maße in Millimeter.

Nullpunkt: x = Laengsrichtung der Platine (0 = USB-Kante), y = Querrichtung,
z = 0 = Oberseite der Platine. Nach oben ist die Anzeigenseite, nach unten die
Akkuseite.

Rundungen (Gehaeuseecken, Loecher) sind hier Quadrate - das ist die Vorschau.
Die Rundungen zeichnet `cad/housing.py` in Fusion 360.
"""

import os

HERE = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(HERE, os.pardir, "parameters.csv")


def read_parameters(path=CSV):
    """Liest parameters.csv und gibt {name: (wert, kommentar)} zurueck."""
    out = {}
    with open(path, "r", encoding="utf-8") as fh:
        head = fh.readline().rstrip("\n").split(";")
        if head[:2] != ["name", "value_mm"]:
            raise ValueError("parameters.csv: Kopfzeile erwartet name;value_mm;comment")
        for lineno, line in enumerate(fh, start=2):
            line = line.rstrip("\n")
            if not line.strip():
                continue
            parts = line.split(";")
            if len(parts) < 2:
                raise ValueError("parameters.csv Zeile %d: zu wenige Felder" % lineno)
            name = parts[0].strip()
            try:
                value = float(parts[1])
            except ValueError:
                raise ValueError("parameters.csv Zeile %d: %r ist keine Zahl" % (lineno, parts[1]))
            out[name] = (value, parts[2].strip() if len(parts) > 2 else "")
    return out


def values(path=CSV):
    return dict((k, v[0]) for k, v in read_parameters(path).items())


def derive(p):
    """Rechnet die abgeleiteten Maße. p ist {name: wert} aus parameters.csv."""
    d = {}
    # Dickenrechnung, siehe cad/masse.md Abschnitt 3
    d["z_pcb_top"] = 0.0
    d["z_pcb_bot"] = -p["pcb_th"]
    d["z_cav_top"] = p["h_top_part"] + p["gap_top"]
    d["z_roof"] = d["z_cav_top"] + p["skin_t"]
    d["z_cav_bot"] = d["z_pcb_bot"] - p["gap_bot"] - p["h_bot_part"]
    d["z_floor"] = d["z_cav_bot"] - p["skin_t"]
    d["total_th"] = d["z_roof"] - d["z_floor"]

    # Grundriss: Tasche = Platine plus Spiel, aussen = Tasche plus Wand
    d["pk_x0"] = -p["pcb_fit"]
    d["pk_x1"] = p["pcb_len"] + p["pcb_fit"]
    d["pk_y0"] = -p["pcb_fit"]
    d["pk_y1"] = p["pcb_wid"] + p["pcb_fit"]
    d["ot_x0"] = d["pk_x0"] - p["wall_t"]
    d["ot_x1"] = d["pk_x1"] + p["wall_t"]
    d["ot_y0"] = d["pk_y0"] - p["wall_t"]
    d["ot_y1"] = d["pk_y1"] + p["wall_t"]
    d["outer_wid"] = d["ot_y1"] - d["ot_y0"]
    d["outer_len"] = d["ot_x1"] - d["ot_x0"]

    # Hohlraum fuer die Bauteile: Platinenrand bleibt ledge_w breit frei
    inset = p["pcb_fit"] + p["ledge_w"]
    d["cv_x0"] = d["pk_x0"] + inset
    d["cv_x1"] = d["pk_x1"] - inset
    d["cv_y0"] = d["pk_y0"] + inset
    d["cv_y1"] = d["pk_y1"] - inset

    # Oese hinter der Platine, am Ende des Antennenfelds
    d["eye_x0"] = d["ot_x1"]
    d["eye_x1"] = d["ot_x1"] + p["eye_len"]
    d["eye_wid"] = 12.0
    d["eye_y0"] = p["pcb_wid"] / 2.0 - d["eye_wid"] / 2.0
    d["eye_y1"] = p["pcb_wid"] / 2.0 + d["eye_wid"] / 2.0
    d["eye_cx"] = d["eye_x0"] + p["eye_len"] / 2.0
    d["eye_cy"] = p["pcb_wid"] / 2.0
    d["overall_len"] = d["eye_x1"] - d["ot_x0"]

    # Der USB4085 ist "PCB Top Mount": sein Koerper sitzt vollstaendig auf der
    # Platine. Er ist hoeher als der Hohlraum, darum wird das Dach vorn zu
    # einer Nase angehoben. Siehe masse.md Abschnitt 9.
    d["usb_z0"] = d["z_pcb_top"]
    d["usb_z1"] = d["z_pcb_top"] + p["usb_open_z"]
    d["z_cav_front"] = p["usb_body_h"] + p["usb_gap_top"]
    d["z_roof_front"] = d["z_cav_front"] + p["skin_t"]
    d["nose_x1"] = p["usb_nose_x"]
    # Mulde um die Frontoeffnung. Sie ist keine Formgebung: ohne sie muesste
    # die Buchse 1,90 mm ueber die Platinenkante ragen, und dann liegen die
    # Bohrloecher ihrer Haltestifte auf der Platinenkante. Siehe masse.md 15.
    d["rec_x1"] = d["ot_x0"] + p["usb_recess_d"]
    d["rec_z1"] = d["usb_z1"] + p["usb_recess_z1"]
    d["d_front"] = -d["ot_x0"] - p["usb_recess_d"]
    d["front_th"] = d["z_roof_front"] - d["z_floor"]

    # Lage der Schnappwuelste in Laengsrichtung
    d["snap_cx"] = [15.0, 45.0]
    return d


def _sq(cx, cy, size, z0, z1):
    """Quadrat als Ersatz fuer ein rundes Loch - nur fuer die Vorschau."""
    h = size / 2.0
    return (cx - h, cx + h, cy - h, cy + h, z0, z1)


def shell_top(p, d, preview=True):
    """Obere Schale: Dach, Fenster, Tasteroeffnung, Aufnahme der Schnappstufe."""
    ops = []
    z0, z1 = d["z_pcb_top"], d["z_roof"]
    ops.append(("+", (d["ot_x0"], d["ot_x1"], d["ot_y0"], d["ot_y1"], z0, z1)))
    ops.append(("+", (d["eye_x0"], d["eye_x1"], d["eye_y0"], d["eye_y1"], z0, z1)))
    # Nase vorn: der USB4085 traegt 3,46 mm auf, der Hohlraum hat nur 2,35.
    # Das Dach steigt ueber dem Stecker auf volle Breite an, mit senkrechter
    # Stufe - die druckt ohne Stuetze und braucht keine Fase.
    ops.append(("+", (d["ot_x0"], d["nose_x1"], d["ot_y0"], d["ot_y1"],
                      z1, d["z_roof_front"])))
    # Aufnahme fuer die Schnappstufe der unteren Schale - vor dem Hohlraum,
    # sonst fuellt das Zurueckgeben des Innenteils den Hohlraum wieder auf
    lip_top = p["lip_h"] + 0.1
    ops.append(("-", (d["ot_x0"], d["ot_x1"], d["ot_y0"], d["ot_y1"], z0, lip_top)))
    ops.append(("+", (d["ot_x0"] + p["lip_t"], d["ot_x1"] - p["lip_t"],
                      d["ot_y0"] + p["lip_t"], d["ot_y1"] - p["lip_t"], z0, lip_top)))
    # Hohlraum fuer die Bauteile der Anzeigenseite
    ops.append(("-", (d["cv_x0"], d["cv_x1"], d["cv_y0"], d["cv_y1"], z0, d["z_cav_top"])))
    # und unter der Nase tiefer, damit der Stecker hineinpasst
    ops.append(("-", (d["cv_x0"], d["nose_x1"], d["cv_y0"], d["cv_y1"],
                      z0, d["z_cav_front"])))
    # Taschen fuer die Schnappwuelste, in der Innenwand der Stufe
    for cx in d["snap_cx"]:
        for side in (0, 1):
            if side == 0:
                y0 = d["ot_y0"] + p["lip_t"]
                y1 = y0 + p["snap_d"]
            else:
                y1 = d["ot_y1"] - p["lip_t"]
                y0 = y1 - p["snap_d"]
            ops.append(("-", (cx - p["snap_len"] / 2.0, cx + p["snap_len"] / 2.0,
                              y0, y1,
                              p["lip_h"] - p["snap_h"] - 0.1, p["lip_h"] - 0.1)))
    # Fenster der Anzeige
    ops.append(("-", (p["oled_cx"] - p["oled_win_x"] / 2.0, p["oled_cx"] + p["oled_win_x"] / 2.0,
                      p["oled_cy"] - p["oled_win_y"] / 2.0, p["oled_cy"] + p["oled_win_y"] / 2.0,
                      d["z_cav_top"], z1)))
    # Lichtfenster der LED - rund, in der Vorschau quadratisch
    if preview:
        ops.append(("-", _sq(p["led_cx"], p["led_cy"], p["led_win_d"], d["z_cav_top"], z1)))
    # Die beiden seitlich betaetigten Taster sitzen am Platinenrand und damit
    # im Klemmkragen - der wird dort auf 1.65 mm Hoehe freigestellt. SW1 steht
    # neben der Anzeige, SW2 seit dem 24.10.2026 am USB-Ende: zwischen Modul
    # und Anzeigeglas fehlten dem Reset 0,5491 mm.
    bx0 = p["btn_cx"] - p["btn_slot_x"] / 2.0
    bx1 = p["btn_cx"] + p["btn_slot_x"] / 2.0
    # Die Freistellung ist laenger als der Schlitz: der Koerper des Tasters
    # wird je nach Quelle 3,5 oder 4,7 mm lang angegeben, beides muss hinein
    r1x0 = p["btn_cx"] - p["btn_relief_x"] / 2.0
    r1x1 = p["btn_cx"] + p["btn_relief_x"] / 2.0
    r2x0 = p["sw2_cx"] - p["btn_relief_x"] / 2.0
    r2x1 = p["sw2_cx"] + p["btn_relief_x"] / 2.0
    ops.append(("-", (r1x0, r1x1, d["pk_y0"], d["cv_y0"], z0, p["btn_relief_z"])))
    ops.append(("-", (r2x0, r2x1, d["cv_y1"], d["pk_y1"], z0, p["btn_relief_z"])))
    # SW1 (Bestaetigen): Schlitz durch die Seitenwand, von aussen zu druecken
    ops.append(("-", (bx0, bx1, d["ot_y0"], d["pk_y0"],
                      max(z0, p["btn_z"] - p["btn_slot_z"] / 2.0),
                      p["btn_z"] + p["btn_slot_z"] / 2.0)))
    # SW2 (Reset): nur eine Blindtasche, die Wand bleibt zu - er darf von
    # aussen nicht erreichbar sein (Vorgabe aus hardware/layout-notes.md)
    ops.append(("-", (r2x0, r2x1, d["pk_y1"], d["pk_y1"] + p["sw2_pocket_d"],
                      z0, p["btn_relief_z"])))
    # Frontoeffnung fuer USB-C - sie reicht in die Nase hinein, darum nicht
    # auf z_roof begrenzen
    ops.append(("-", (d["ot_x0"], d["cv_x0"],
                      p["usb_cy"] - p["usb_open_y"] / 2.0, p["usb_cy"] + p["usb_open_y"] / 2.0,
                      z0, d["usb_z1"])))
    # Mulde um die Oeffnung, damit die Buchse weniger weit vorstehen muss
    ops.append(("-", (d["ot_x0"], d["rec_x1"],
                      p["usb_cy"] - p["usb_recess_w"] / 2.0,
                      p["usb_cy"] + p["usb_recess_w"] / 2.0,
                      z0, d["rec_z1"])))
    # Loch der Oese - rund, in der Vorschau quadratisch
    if preview:
        ops.append(("-", _sq(d["eye_cx"], d["eye_cy"], p["eye_hole_d"], z0, z1)))
    return ops


def shell_bottom(p, d, preview=True):
    """Untere Schale: Boden, Platinentasche, Akkuraum, Sensoroeffnung, Stufe."""
    ops = []
    z0, z1 = d["z_floor"], d["z_pcb_top"]
    ops.append(("+", (d["ot_x0"], d["ot_x1"], d["ot_y0"], d["ot_y1"], z0, z1)))
    ops.append(("+", (d["eye_x0"], d["eye_x1"], d["eye_y0"], d["eye_y1"], z0, z1)))
    # Schnappstufe: aussere Haelfte der Wand steigt ueber die Trennebene
    ops.append(("+", (d["ot_x0"], d["ot_x1"], d["ot_y0"], d["ot_y1"], z1, z1 + p["lip_h"])))
    ops.append(("-", (d["ot_x0"] + p["lip_t"], d["ot_x1"] - p["lip_t"],
                      d["ot_y0"] + p["lip_t"], d["ot_y1"] - p["lip_t"], z1, z1 + p["lip_h"])))
    ops.append(("+", (d["eye_x0"], d["eye_x1"], d["eye_y0"], d["eye_y1"], z1, z1 + p["lip_h"])))
    # Schnappwuelste auf der Innenseite der Stufe
    for cx in d["snap_cx"]:
        for side in (0, 1):
            if side == 0:
                y1b = d["ot_y0"] + p["lip_t"]
                y0b = y1b - p["snap_d"]
            else:
                y0b = d["ot_y1"] - p["lip_t"]
                y1b = y0b + p["snap_d"]
            ops.append(("+", (cx - p["snap_len"] / 2.0, cx + p["snap_len"] / 2.0,
                              y0b, y1b,
                              p["lip_h"] - p["snap_h"], p["lip_h"])))
    # Tasche fuer die Platine
    ops.append(("-", (d["pk_x0"], d["pk_x1"], d["pk_y0"], d["pk_y1"], d["z_pcb_bot"], z1)))
    # Raum fuer Ferritfolie, Akku und die flachen Bauteile
    ops.append(("-", (d["cv_x0"], d["cv_x1"], d["cv_y0"], d["cv_y1"], d["z_cav_bot"], d["z_pcb_bot"])))
    # Sitz fuer den Fingerabdrucksensor: ein glattes Loch durch den Boden.
    # Der Sensor ist ein Flex-Modul und wird von innen eingeklebt, mit seiner
    # Flaeche buendig nach aussen - auf der Platine waere er 4,82 mm vom
    # Finger entfernt und damit nutzlos. Siehe masse.md Abschnitt 14.
    if preview:
        ops.append(("-", _sq(p["fp_cx"], p["fp_cy"], p["fp_open_d"], z0, d["z_cav_bot"])))
    # Frontoeffnung fuer USB-C. Sie reicht ueber die Trennebene, also muss
    # auch die Schnappstufe dort weg - sonst steht der Falz vor der Oeffnung.
    lip_z = z1 + p["lip_h"]
    ops.append(("-", (d["ot_x0"], d["cv_x0"],
                      p["usb_cy"] - p["usb_open_y"] / 2.0, p["usb_cy"] + p["usb_open_y"] / 2.0,
                      max(z0, d["usb_z0"]), min(lip_z, max(z1, d["usb_z1"])))))
    # SW1: der Teil des Schlitzes unter der Trennebene und durch die Stufe
    # Mulde um die Frontoeffnung, der Teil unter der Trennebene und durch die
    # Schnappstufe - sonst steht der Falz im Muldengrund
    ops.append(("-", (d["ot_x0"], d["rec_x1"],
                      p["usb_cy"] - p["usb_recess_w"] / 2.0,
                      p["usb_cy"] + p["usb_recess_w"] / 2.0,
                      p["usb_recess_z0"], lip_z)))
    bx0 = p["btn_cx"] - p["btn_slot_x"] / 2.0
    bx1 = p["btn_cx"] + p["btn_slot_x"] / 2.0
    btn_lo = p["btn_z"] - p["btn_slot_z"] / 2.0
    btn_hi = min(lip_z, p["btn_z"] + p["btn_slot_z"] / 2.0)
    if btn_lo < btn_hi:
        ops.append(("-", (bx0, bx1, d["ot_y0"], d["pk_y0"], btn_lo, btn_hi)))
    # Loch der Oese - rund
    if preview:
        ops.append(("-", _sq(d["eye_cx"], d["eye_cy"], p["eye_hole_d"], z0, z1 + p["lip_h"])))
    return ops


def sensor_plug(p, d, preview=True):
    """Blindstueck, das den Sensorsitz schliesst, solange kein Sensor da ist.

    Es wird von innen eingesetzt wie der Sensor selbst. Der Bund liegt innen
    an und haelt es fest - aussen schliesst es buendig mit dem Boden ab.
    """
    ops = []
    body = p["fp_open_d"] - p["fp_plug_fit"]
    flange = p["fp_open_d"] + 1.5
    z0 = d["z_floor"]
    z1 = z0 + p["skin_t"]
    if preview:
        ops.append(("+", _sq(p["fp_cx"], p["fp_cy"], body, z0, z1)))
        ops.append(("+", _sq(p["fp_cx"], p["fp_cy"], flange, z1, z1 + 0.4)))
    return ops


BODIES = [
    ("shell-top", shell_top),
    ("shell-bottom", shell_bottom),
    ("sensor-plug", sensor_plug),
]
