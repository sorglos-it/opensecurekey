"""Liegt an Punkt (x|y) Kupfer? Beantwortet am Gerber, Lage fuer Lage.

Der Gerber ist, was der Fertiger bekommt - jede Vorschau ist nur eine
Darstellung davon. EasyEDA schreibt je Lage EINE gefuellte Region
(G36...G37) als eine einzige geschlossene Kontur, die sich um jede
Aussparung herumwebt; Boegen werden hier mit acht Sehnen angenaehert.

WICHTIG: Vor jedem Ergebnis die Kontrollpunkte pruefen, deren Antwort
vorher feststeht. Eine Pruefung, die man nicht zum Anschlagen und zum
Danebenliegen gebracht hat, ist keine.

Aufruf:  python gerber-kupfer.py <Gerberdatei> <x> <y> [<x> <y> ...]
"""
import re, math, sys

SKALA = 10**5          # 4 Vorkomma, 5 Nachkomma, Millimeter

def kontur(pfad):
    """Stuetzpunkte der gefuellten Region, Boegen in acht Sehnen."""
    inregion = False
    punkte = []
    x = y = 0.0
    for zeile in open(pfad, encoding="utf-8", errors="replace"):
        if "G36" in zeile:
            inregion = True
            continue
        if "G37" in zeile:
            break
        if not inregion:
            continue
        mx = re.search(r"X(-?\d+)", zeile)
        my = re.search(r"Y(-?\d+)", zeile)
        mi = re.search(r"I(-?\d+)", zeile)
        mj = re.search(r"J(-?\d+)", zeile)
        nx = int(mx.group(1)) / SKALA if mx else x
        ny = int(my.group(1)) / SKALA if my else y
        if "D02" in zeile:
            punkte.append((nx, ny))
        elif "D01" in zeile:
            bogen = ("G02" in zeile or "G03" in zeile) and mi and mj
            if bogen:
                cx = x + int(mi.group(1)) / SKALA
                cy = y + int(mj.group(1)) / SKALA
                r = math.hypot(x - cx, y - cy)
                a0 = math.atan2(y - cy, x - cx)
                a1 = math.atan2(ny - cy, nx - cx)
                d = a1 - a0
                if "G02" in zeile and d > 0:
                    d -= 2 * math.pi
                if "G03" in zeile and d < 0:
                    d += 2 * math.pi
                for k in range(1, 9):
                    a = a0 + d * k / 8
                    punkte.append((cx + r * math.cos(a), cy + r * math.sin(a)))
            else:
                punkte.append((nx, ny))
        x, y = nx, ny
    return punkte

def innen(punkte, p):
    """Strahlensatz: ungerade Zahl von Kreuzungen heisst innerhalb."""
    px, py = p
    drin = False
    n = len(punkte)
    for i in range(n):
        x1, y1 = punkte[i]
        x2, y2 = punkte[(i + 1) % n]
        if (y1 > py) != (y2 > py):
            if px < (x2 - x1) * (py - y1) / (y2 - y1) + x1:
                drin = not drin
    return drin

if __name__ == "__main__":
    datei = sys.argv[1]
    werte = [float(a) for a in sys.argv[2:]]
    pk = kontur(datei)
    print("%s: %d Stuetzpunkte" % (datei, len(pk)))
    for i in range(0, len(werte), 2):
        p = (werte[i], werte[i + 1])
        print("   %8.4f / %8.4f  ->  %s" % (p[0], p[1], "KUPFER" if innen(pk, p) else "frei"))
