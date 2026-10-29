"""Geometrie des Gehaeuses aus der Parametertabelle.

Neu gezeichnet am 30.10.2026, nicht ausgebessert. Der Entwurf davor hatte einen
Zellenraum; der Akku ist am 25.10.2026 gestrichen (docs/plan.md). Es ist keine
Zahl und keine Zeile des alten Entwurfs uebernommen.

Keine Abhaengigkeiten. Liefert fuer jeden Koerper eine Liste von Quadern mit
einem Zeichen davor: '+' hinzufuegen, '-' abziehen, IN DIESER REIHENFOLGE.
Alle Masse in Millimeter.

Nullpunkt und Achsen - gleich wie in hardware/, damit es nur eine Lesart gibt:

    x = QUER     0 ... 27,00 mm   (Breite der Platine)
    y = LAENGS   0 ... 59,55 mm   (0 = USB-Kante)
    z = 0        MITTELEBENE der Platine, und damit auch Mittelebene des
                 Steckers J1 - er ist ein Straddle-Mount, die Platine sitzt
                 mittig in ihm (docs/usb-stecker.md).

Der alte Entwurf hatte x als Laengsrichtung. Das war die zweite Lesart, die
sich dieses Projekt nicht mehr leistet.

Rundungen (Gehaeuseecken, Lichtfenster, Oesenloch, Einguesse) sind hier
Quadrate - das ist die Vorschau. Die runde Fassung macht cad/housing.py in
Fusion 360.

Die Hoehenrechnung steht nicht hier, sondern in budget.py.
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
                raise ValueError("parameters.csv Zeile %d: %r ist keine Zahl"
                                 % (lineno, parts[1]))
            out[name] = (value, parts[2].strip() if len(parts) > 2 else "")
    return out


def values(path=CSV):
    return dict((k, v[0]) for k, v in read_parameters(path).items())


def derive(p):
    """Rechnet die abgeleiteten Masse. p ist {name: wert} aus parameters.csv."""
    d = {}

    # --- Hoehe. Nullpunkt ist die Platinenmittelebene; Einzelheiten budget.py
    d["z_pcb_top"] = p["pcb_th"] / 2.0
    d["z_pcb_bot"] = -p["pcb_th"] / 2.0
    d["h_pot_top"] = p["h_top_part"] + p["pot_cover"]
    d["z_pot_top"] = d["z_pcb_top"] + d["h_pot_top"]
    d["z_roof"] = d["z_pot_top"] + p["skin_t"]
    # freie Hoehe unten: das hoechste TROCKENE Teil (microSD-Schacht) oder das
    # hoechste vergossene Teil samt Deckung - was mehr braucht
    d["h_free_bot"] = max(p["h_bot_part_b"], p["h_bot_potted"] + p["pot_cover"])
    d["z_pot_bot"] = d["z_pcb_bot"] - d["h_free_bot"]
    d["z_floor"] = d["z_pot_bot"] - p["skin_t"]
    d["total_th"] = d["z_roof"] - d["z_floor"]
    # Mitte des Gehaeuses in z. Weil z = 0 die Steckermittelebene ist, ist
    # dieser Wert unmittelbar der Versatz zwischen Stecker- und Gehaeusemitte.
    d["plug_offset"] = (d["z_roof"] + d["z_floor"]) / 2.0

    # --- Grundriss. Tasche = Platine plus Spiel, aussen = Tasche plus Wand
    d["pk_x0"] = -p["pcb_fit"]
    d["pk_x1"] = p["pcb_wid"] + p["pcb_fit"]
    d["pk_y0"] = -p["pcb_fit"]
    d["pk_y1"] = p["pcb_len"] + p["pcb_fit"]
    d["ot_x0"] = d["pk_x0"] - p["wall_t"]
    d["ot_x1"] = d["pk_x1"] + p["wall_t"]
    d["ot_y0"] = d["pk_y0"] - p["wall_t"]
    d["ot_y1"] = d["pk_y1"] + p["wall_t"]
    d["outer_wid"] = d["ot_x1"] - d["ot_x0"]
    d["outer_len"] = d["ot_y1"] - d["ot_y0"]

    # Vergussraum: ringsum bleibt seal_w als Auflage stehen. Sie traegt die
    # Platine und trennt den oberen vom unteren Vergussraum.
    d["cv_x0"] = d["pk_x0"] + p["seal_w"]
    d["cv_x1"] = d["pk_x1"] - p["seal_w"]
    d["cv_y0"] = d["pk_y0"] + p["seal_w"]
    d["cv_y1"] = d["pk_y1"] - p["seal_w"]

    # --- Oese hinter der Platine, am Ende des Antennenfelds
    d["eye_y0"] = d["ot_y1"]
    d["eye_y1"] = d["ot_y1"] + p["eye_len"]
    d["eye_x0"] = p["pcb_wid"] / 2.0 - p["eye_wid"] / 2.0
    d["eye_x1"] = p["pcb_wid"] / 2.0 + p["eye_wid"] / 2.0
    d["eye_cx"] = p["pcb_wid"] / 2.0
    d["eye_cy"] = d["eye_y0"] + p["eye_len"] / 2.0
    d["overall_len"] = d["eye_y1"] - d["ot_y0"]

    # --- Kragen um die Steckerhuelle von J1. Er fasst den Stecker; die
    # Zugkraft darf nicht in die 0,8-mm-Platine gehen (docs/usb-stecker.md).
    d["usb_cx"] = p["pcb_wid"] / 2.0
    d["usb_x0"] = d["usb_cx"] - p["usb_shell_w"] / 2.0 - p["usb_collar_fit"]
    d["usb_x1"] = d["usb_cx"] + p["usb_shell_w"] / 2.0 + p["usb_collar_fit"]
    d["usb_z0"] = -p["usb_shell_h"] / 2.0 - p["usb_collar_fit"]
    d["usb_z1"] = p["usb_shell_h"] / 2.0 + p["usb_collar_fit"]
    # so weit reicht der Stecker auf die Platine: Loetbildtiefe plus Luft
    d["usb_y1"] = p["usb_pad_depth"] + p["pot_island_fit"]

    # --- Anzeigefenster: aktive Flaeche plus Rand, Loch im Dach
    d["win_x0"] = p["oled_cx"] - p["oled_act_x"] / 2.0 - p["oled_win_margin"]
    d["win_x1"] = p["oled_cx"] + p["oled_act_x"] / 2.0 + p["oled_win_margin"]
    d["win_cy"] = (p["oled_glass_y0"] + p["oled_glass_y1"]) / 2.0
    d["win_y0"] = d["win_cy"] - p["oled_act_y"] / 2.0 - p["oled_win_margin"]
    d["win_y1"] = d["win_cy"] + p["oled_act_y"] / 2.0 + p["oled_win_margin"]

    # --- D1 wandert mit der Spitze der Platine, darum vom hinteren Rand aus
    d["led_cy"] = p["pcb_len"] - p["led_back"]

    # --- Einguesse und Entlueftungen im Dach, ueber dem Vergussraum
    d["ports"] = [(p["oled_cx"], 6.0), (p["oled_cx"], 50.0)]
    d["vents"] = [(3.0, 15.0), (24.0, 50.0)]
    return d


# --- Trockeninseln ----------------------------------------------------------
#
# Vergossen wird ALLES, was nicht hinter einem Damm liegt. Diese Regel haengt
# nicht an Bauteillagen, sondern nur an den Oeffnungen - und die sind Sache des
# Gehaeuses. Darum uebersteht sie eine Umplatzierung auf der Platine.
#
# Eine Insel ist (name, (x0, x1, y0, y1), [Seiten mit Damm], Dammdicke).
# Seiten: 'x0' 'x1' 'y0' 'y1'. Wo die Gehaeusewand selbst dichtet, steht kein
# Damm.

def islands_top(p, d):
    fit = p["pot_island_fit"]
    t = p["pot_dam_t"]
    glass_x0 = p["oled_cx"] - p["oled_glass_x"] / 2.0
    glass_x1 = p["oled_cx"] + p["oled_glass_x"] / 2.0
    return [
        # Der Stecker darf keinen Verguss in sein Kontaktfeld bekommen.
        ("usb", (d["usb_x0"], d["usb_x1"], d["pk_y0"], d["usb_y1"]),
         ["x0", "x1", "y1"], t),
        # Anzeigeglas. Am vorderen Rand sind zwischen U1 (endet y 22,55) und
        # dem Glas (beginnt 23,05) nur 0,50 mm - dort ist der Damm 0,40 mm
        # duenn und steht mit 0,10 mm Luft vor U1.
        ("oled", (glass_x0 - fit, glass_x1 + fit,
                  p["oled_glass_y0"], p["oled_glass_y1"] + fit),
         ["x0", "x1", "y1"], t),
        # Lichtfenster der LED
        ("led", (p["led_cx"] - p["led_win_d"] / 2.0 - 0.5,
                 p["led_cx"] + p["led_win_d"] / 2.0 + 0.5,
                 d["led_cy"] - p["led_win_d"] / 2.0 - 0.5,
                 d["led_cy"] + p["led_win_d"] / 2.0 + 0.5),
         ["x0", "x1", "y0", "y1"], t),
        # SW1: Betaetigung zur Kante x = 0, Schlitz in der Wand
        ("sw1", (d["pk_x0"], 5.5,
                 p["sw1_cy"] - p["btn_relief_y"] / 2.0,
                 p["sw1_cy"] + p["btn_relief_y"] / 2.0),
         ["x1", "y0", "y1"], t),
        # SW2: Betaetigung zur Kante x = 27, Wand bleibt zu
        ("sw2", (p["sw2_island_x0"], d["pk_x1"],
                 p["sw2_cy"] - p["btn_relief_y"] / 2.0,
                 p["sw2_cy"] + p["btn_relief_y"] / 2.0),
         ["x0", "y0", "y1"], t),
    ]


def oled_front_dam(p, d):
    """Die vordere Dammwand des Anzeigeglases - duenner als die anderen.

    Sie steht in dem 0,50-mm-Spalt zwischen U1 und dem Glas, darum
    pot_dam_t_min statt pot_dam_t.
    """
    glass_x0 = p["oled_cx"] - p["oled_glass_x"] / 2.0
    glass_x1 = p["oled_cx"] + p["oled_glass_x"] / 2.0
    fit, t = p["pot_island_fit"], p["pot_dam_t"]
    return (glass_x0 - fit - t, glass_x1 + fit + t,
            p["oled_glass_y0"] - p["pot_dam_t_min"], p["oled_glass_y0"])


def islands_bottom(p, d):
    fit = p["pot_island_fit"]
    t = p["pot_dam_t"]
    return [
        ("usb", (d["usb_x0"], d["usb_x1"], d["pk_y0"], d["usb_y1"]),
         ["x0", "x1", "y1"], t),
        # microSD-Schacht: die Karte muss hinaus, also bleibt der Schacht
        # trocken. Variante B; in Variante A entfaellt diese Insel.
        ("microsd", (d["pk_x0"], p["sd_body_x1"] + fit,
                     p["sd_body_y0"] - fit, p["sd_body_y1"] + fit),
         ["x1", "y0", "y1"], t),
    ]


def _dam_boxes(island, z0, z1):
    """Die Dammwaende einer Insel als Quader."""
    _name, (x0, x1, y0, y1), seiten, t = island
    if not seiten or t <= 0.0:
        return []
    out = []
    ax0 = x0 - (t if "x0" in seiten else 0.0)
    ax1 = x1 + (t if "x1" in seiten else 0.0)
    if "x0" in seiten:
        out.append((x0 - t, x0, y0 - t, y1 + t, z0, z1))
    if "x1" in seiten:
        out.append((x1, x1 + t, y0 - t, y1 + t, z0, z1))
    if "y0" in seiten:
        out.append((ax0, ax1, y0 - t, y0, z0, z1))
    if "y1" in seiten:
        out.append((ax0, ax1, y1, y1 + t, z0, z1))
    return out


def _ledge_cut(island, d, z0, z1):
    """Wo eine Insel bis an die Platinenkante reicht, muss die Auflage weg."""
    _name, (x0, x1, y0, y1), _seiten, _t = island
    out = []
    if x0 <= d["pk_x0"] + 1e-9:
        out.append((d["pk_x0"], d["cv_x0"], y0, y1, z0, z1))
    if x1 >= d["pk_x1"] - 1e-9:
        out.append((d["cv_x1"], d["pk_x1"], y0, y1, z0, z1))
    if y0 <= d["pk_y0"] + 1e-9:
        out.append((x0, x1, d["pk_y0"], d["cv_y0"], z0, z1))
    if y1 >= d["pk_y1"] - 1e-9:
        out.append((x0, x1, d["cv_y1"], d["pk_y1"], z0, z1))
    return out


def _sq(cx, cy, size, z0, z1):
    """Quadrat als Ersatz fuer ein rundes Loch - nur fuer die Vorschau."""
    h = size / 2.0
    return (cx - h, cx + h, cy - h, cy + h, z0, z1)


def shell_top(p, d, preview=True):
    """Obere Schale: Dach, Vergussraum, Daemme, Fenster, Einguesse.

    Kein Schnapphaken und keine Schraube: der Deckel darf nicht abnehmbar sein
    (docs/plan.md, FIPS-Punkt 2). Die Fuge ist ein Klebefalz, und der Verguss
    haelt zusaetzlich.
    """
    ops = []
    z0, z1 = d["z_pcb_top"], d["z_roof"]
    ops.append(("+", (d["ot_x0"], d["ot_x1"], d["ot_y0"], d["ot_y1"], z0, z1)))
    ops.append(("+", (d["eye_x0"], d["eye_x1"], d["eye_y0"], d["eye_y1"], z0, z1)))
    # Klebefalz: die obere Schale gibt die INNERE Haelfte der Wand her
    lap_top = z0 + p["join_lap"]
    ops.append(("-", (d["ot_x0"], d["ot_x1"], d["ot_y0"], d["ot_y1"], z0, lap_top)))
    ops.append(("+", (d["ot_x0"] + p["join_lap_t"], d["ot_x1"] - p["join_lap_t"],
                      d["ot_y0"] + p["join_lap_t"], d["ot_y1"] - p["join_lap_t"],
                      z0, lap_top)))
    # Vergussraum
    ops.append(("-", (d["cv_x0"], d["cv_x1"], d["cv_y0"], d["cv_y1"], z0, d["z_pot_top"])))
    # Auflage wegnehmen, wo eine Insel bis an die Platinenkante reicht
    for island in islands_top(p, d):
        for box in _ledge_cut(island, d, z0, d["z_pot_top"]):
            ops.append(("-", box))
    # Daemme
    for island in islands_top(p, d):
        for box in _dam_boxes(island, z0, d["z_pot_top"]):
            ops.append(("+", box))
    x0, x1, y0, y1 = oled_front_dam(p, d)
    ops.append(("+", (x0, x1, y0, y1, z0, d["z_pot_top"])))
    # Anzeigefenster: glatter Durchbruch im Dach, ohne nach unten gezogenen Rand
    ops.append(("-", (d["win_x0"], d["win_x1"], d["win_y0"], d["win_y1"],
                      d["z_pot_top"], z1)))
    # Lichtfenster der LED - rund, in der Vorschau quadratisch
    if preview:
        ops.append(("-", _sq(p["led_cx"], d["led_cy"], p["led_win_d"],
                             d["z_pot_top"], z1)))
    # SW1: Schlitz durch die Seitenwand, von aussen zu druecken
    ops.append(("-", (d["ot_x0"], d["pk_x0"],
                      p["sw1_cy"] - p["btn_slot_y"] / 2.0,
                      p["sw1_cy"] + p["btn_slot_y"] / 2.0,
                      max(z0, d["z_pcb_top"] + p["btn_z"] - p["btn_slot_z"] / 2.0),
                      d["z_pcb_top"] + p["btn_z"] + p["btn_slot_z"] / 2.0)))
    # SW2: nur eine Blindtasche, die Wand bleibt stehen
    ops.append(("-", (d["pk_x1"], d["pk_x1"] + p["sw2_pocket_d"],
                      p["sw2_cy"] - p["btn_relief_y"] / 2.0,
                      p["sw2_cy"] + p["btn_relief_y"] / 2.0,
                      z0, d["z_pcb_top"] + p["btn_z"] + p["btn_slot_z"] / 2.0)))
    # Kragen um die Steckerhuelle: Oeffnung durch die Frontwand
    ops.append(("-", (d["usb_x0"], d["usb_x1"], d["ot_y0"], d["cv_y0"],
                      z0, max(z0 + 0.01, d["usb_z1"]))))
    # microSD-Schlitz, der Teil oberhalb der Trennebene
    sd_hi = p["sd_slot_z"] / 2.0
    if sd_hi > z0:
        ops.append(("-", (d["ot_x0"], d["pk_x0"], p["sd_slot_y0"], p["sd_slot_y1"],
                          z0, sd_hi)))
    # Einguesse und Entlueftungen - rund, in der Vorschau quadratisch
    if preview:
        for cx, cy in d["ports"]:
            ops.append(("-", _sq(cx, cy, p["pot_port_d"], d["z_pot_top"], z1)))
        for cx, cy in d["vents"]:
            ops.append(("-", _sq(cx, cy, p["pot_vent_d"], d["z_pot_top"], z1)))
        ops.append(("-", _sq(d["eye_cx"], d["eye_cy"], p["eye_hole_d"], z0, z1)))
    return ops


def shell_bottom(p, d, preview=True):
    """Untere Schale: Boden, Auflage, Vergussraum, Daemme, Schlitz, Klebefalz."""
    ops = []
    z0, z1 = d["z_floor"], d["z_pcb_top"]
    ops.append(("+", (d["ot_x0"], d["ot_x1"], d["ot_y0"], d["ot_y1"], z0, z1)))
    ops.append(("+", (d["eye_x0"], d["eye_x1"], d["eye_y0"], d["eye_y1"], z0, z1)))
    # Klebefalz: die untere Schale gibt die AEUSSERE Haelfte der Wand her
    lap_top = z1 + p["join_lap"]
    ops.append(("+", (d["ot_x0"], d["ot_x1"], d["ot_y0"], d["ot_y1"], z1, lap_top)))
    ops.append(("-", (d["ot_x0"] + p["join_lap_t"], d["ot_x1"] - p["join_lap_t"],
                      d["ot_y0"] + p["join_lap_t"], d["ot_y1"] - p["join_lap_t"],
                      z1, lap_top)))
    ops.append(("+", (d["eye_x0"], d["eye_x1"], d["eye_y0"], d["eye_y1"], z1, lap_top)))
    # Tasche fuer die Platine - sie liegt auf der Auflage ringsum
    ops.append(("-", (d["pk_x0"], d["pk_x1"], d["pk_y0"], d["pk_y1"],
                      d["z_pcb_bot"], z1)))
    # Vergussraum unten
    ops.append(("-", (d["cv_x0"], d["cv_x1"], d["cv_y0"], d["cv_y1"],
                      d["z_pot_bot"], d["z_pcb_bot"])))
    for island in islands_bottom(p, d):
        for box in _ledge_cut(island, d, d["z_pot_bot"], d["z_pcb_bot"]):
            ops.append(("-", box))
    for island in islands_bottom(p, d):
        for box in _dam_boxes(island, d["z_pot_bot"], d["z_pcb_bot"]):
            ops.append(("+", box))
    # Kragen um die Steckerhuelle, Teil unter der Trennebene
    ops.append(("-", (d["usb_x0"], d["usb_x1"], d["ot_y0"], d["cv_y0"],
                      max(z0, d["usb_z0"]), min(lap_top, d["usb_z1"]))))
    # microSD-Schlitz durch die Seitenwand
    ops.append(("-", (d["ot_x0"], d["pk_x0"], p["sd_slot_y0"], p["sd_slot_y1"],
                      -p["sd_slot_z"] / 2.0, min(lap_top, p["sd_slot_z"] / 2.0))))
    # SW1: der Teil des Schlitzes unter der Trennebene und durch den Falz
    btn_lo = d["z_pcb_top"] + p["btn_z"] - p["btn_slot_z"] / 2.0
    btn_hi = min(lap_top, d["z_pcb_top"] + p["btn_z"] + p["btn_slot_z"] / 2.0)
    if btn_lo < btn_hi:
        ops.append(("-", (d["ot_x0"], d["pk_x0"],
                          p["sw1_cy"] - p["btn_slot_y"] / 2.0,
                          p["sw1_cy"] + p["btn_slot_y"] / 2.0, btn_lo, btn_hi)))
    # Loch der Oese - rund, in der Vorschau quadratisch
    if preview:
        ops.append(("-", _sq(d["eye_cx"], d["eye_cy"], p["eye_hole_d"], z0, lap_top)))
    return ops


BODIES = [
    ("shell-top", shell_top),
    ("shell-bottom", shell_bottom),
]
