# -*- coding: utf-8 -*-
"""Hoehenrechnung des Gehaeuses - jede Lagensumme einzeln ausgewiesen.

    python cad/tools/budget.py
    python cad/tools/budget.py --selftest

Nullpunkt der Hoehe ist die **Mittelebene der Platine**. Das ist nicht Willkuer:
J1 ist ein Straddle-Mount-Stecker, die Platine sitzt mittig in ihm
(docs/usb-stecker.md). Die Mittelebene der Platine ist also auch die
Mittelebene des Steckers, und nur von dort aus ist ablesbar, ob der Stecker
mittig im Gehaeuse steht.

Was hier NICHT gerechnet wird: Laengen und Breiten. Die stehen in geometry.py.

Alle Zahlen kommen aus ../parameters.csv. Standardbibliothek, keine
Abhaengigkeiten.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import geometry  # noqa: E402

# Die 2,90 mm aus hardware/layout-notes.md. Sie messen die Hoehe der
# Steckerhuelle von J1 UEBER ALLES, symmetrisch zur Platinenmittelebene -
# also das, was in eine USB-Buchse passen muss. Sie sind KEINE Gehaeusedicke.
# Warum die Unterscheidung noetig ist, steht in masse.md Abschnitt 3.
GRENZE = "usb_shell_h"

TOL = 1e-9


class Fall(object):
    """Ein Bestueckungsfall: was oben steht, was unten steht, was vergossen ist."""

    def __init__(self, name, top_part, bot_dammed, bot_potted, pot_cover, erklaerung):
        self.name = name
        self.top_part = top_part        # ueber der Platinen-OBERSEITE, nach +z
        self.bot_dammed = bot_dammed    # unter der Platine, hinter einem Damm (trocken)
        self.bot_potted = bot_potted    # unter der Platine, im Verguss
        self.pot_cover = pot_cover      # Verguss ueber dem hoechsten vergossenen Teil
        self.erklaerung = erklaerung


def faelle(p):
    """Die Faelle, die gerechnet werden. Alle Zahlen aus parameters.csv."""
    cover = p["pot_cover"]
    return [
        Fall("leer", 0.0, 0.0, 0.0, 0.0,
             "Kontrollpunkt: nur Dach, Platine, Boden - kein Bauteil, kein Verguss"),
        Fall("stufe1-a", p["h_top_part"], 0.0,
             max(p["h_bot_potted"], p["h_bot_part_a"]), cover,
             "Stufe 1, Datentraeger Variante A (SD-NAND geloetet, kein Schlitz)"),
        Fall("stufe1-b", p["h_top_part"], p["h_bot_part_b"], p["h_bot_potted"], cover,
             "Stufe 1, Datentraeger Variante B (microSD-Schacht hinter einem Damm)"),
        Fall("stufe2-a", p["h_top_part"], p["h_zif_stufe2"], p["h_bot_potted"], cover,
             "Stufe 2 mit Variante A: ZIF-Stecker des Sensor-Flachbands trocken"),
        Fall("stufe2-b", p["h_top_part"],
             max(p["h_bot_part_b"], p["h_zif_stufe2"]), p["h_bot_potted"], cover,
             "Stufe 2 mit Variante B - Flaechenkonflikt, siehe masse.md Abschnitt 6"),
        Fall("bm-lite", p["h_top_part"], 2.8, p["h_bot_potted"], cover,
             "Gegenprobe: nicht der FPC2534, sondern das 2,8 mm hohe BM-Lite-Modul"),
    ]


def lagen(p, fall):
    """Die beiden Lagenlisten, von der Platinenmittelebene nach aussen.

    Liefert (oben, unten); jede Liste ist [(Lage, was es misst, mm)].
    """
    halb = p["pcb_th"] / 2.0
    oben = [
        ("halbe Platine", "Platinenmittelebene bis Platinenoberseite", halb),
        ("hoechstes Bauteil oben", "Platinenoberseite bis Bauteiloberkante", fall.top_part),
        ("Verguss darueber", "Bauteiloberkante bis Vergussoberflaeche", fall.pot_cover),
        ("Dach", "Vergussoberflaeche bis Aussenflaeche oben", p["skin_t"]),
    ]
    frei = max(fall.bot_dammed, fall.bot_potted + fall.pot_cover)
    unten = [
        ("halbe Platine", "Platinenmittelebene bis Platinenunterseite", halb),
        ("freie Hoehe unten", "Platinenunterseite bis Bodeninnenflaeche: hoechstes "
                              "trockenes Teil ODER vergossenes Teil plus Deckung", frei),
        ("Boden", "Bodeninnenflaeche bis Aussenflaeche unten", p["skin_t"]),
    ]
    return oben, unten


def summe(liste):
    return sum(v for _n, _w, v in liste)


def rechne(p, fall):
    """Ergebnis eines Falls. 'dicke' ist die Gehaeusedicke, nicht die Steckerhoehe."""
    oben, unten = lagen(p, fall)
    h_oben, h_unten = summe(oben), summe(unten)
    return {
        "name": fall.name,
        "oben": oben,
        "unten": unten,
        "h_oben": h_oben,
        "h_unten": h_unten,
        "dicke": h_oben + h_unten,
        # Wie weit die Steckermittelebene aus der Gehaeusemittelebene wandert
        "versatz": (h_oben - h_unten) / 2.0,
        "erklaerung": fall.erklaerung,
    }


# --- Pruefungen. Jede liefert (name, bestanden, text) ------------------------

def pruefe_grenze(p, r):
    """Bleibt die Lagensumme unter der Grenze aus hardware/layout-notes.md?"""
    grenze = p[GRENZE]
    ok = r["dicke"] <= grenze + TOL
    return ("Lagensumme unter %.2f mm" % grenze, ok,
            "%s: %.3f mm gegen %.2f mm" % (r["name"], r["dicke"], grenze))


def pruefe_summe(p, r):
    """Summenprobe: die Dicke muss gleich der Summe ALLER Lagen sein.

    Das ist die Probe gegen den Fehler, der diesem Projekt zweimal passiert ist:
    eine Lage steht in der Aufteilung, fehlt aber in der Summenzeile.
    """
    einzeln = summe(r["oben"]) + summe(r["unten"])
    ok = abs(einzeln - r["dicke"]) <= 1e-9
    return ("Summenprobe", ok, "%s: %.6f gegen %.6f mm" % (r["name"], einzeln, r["dicke"]))


def pruefe_bauteil(p, r):
    """Bleibt jedes EINZELNE Bauteil unter der Grenze?

    Das ist die Probe, die hardware/layout-notes.md meint, wenn dort steht
    'kein Teil hoeher als der Stecker'. Sie vergleicht Bauteilhoehen ueber
    der Platine - nicht Gehaeusedicken.
    """
    grenze = p[GRENZE]
    kandidaten = [r["oben"][1], r["unten"][1]]
    zu_hoch = [(n, v) for n, _w, v in kandidaten if v > grenze + TOL]
    ok = not zu_hoch
    return ("Bauteile unter %.2f mm" % grenze, ok,
            "%s: %s" % (r["name"],
                        ", ".join("%s %.2f" % t for t in zu_hoch) or "alle darunter"))


def pruefe_mittig(p, r, toleranz=0.5):
    """Steht die Steckermittelebene mittig genug im Gehaeuse?

    Die Toleranz ist 0,50 mm, und sie ist gesetzt, nicht gemessen: so viel
    darf das Gehaeuse aus der Steckerachse wandern, bevor es an der Blende
    einer versenkten USB-Buchse streifen koennte. Am Muster zu pruefen.
    """
    ok = abs(r["versatz"]) <= toleranz + TOL
    return ("Stecker mittig (+/- %.2f mm)" % toleranz, ok,
            "%s: Versatz %+.3f mm" % (r["name"], r["versatz"]))


PRUEFUNGEN = (pruefe_grenze, pruefe_summe, pruefe_bauteil, pruefe_mittig)


# --- Waerme -----------------------------------------------------------------

# Spitzenleistung: 67,5 mA im unguenstigsten Fall (docs/power-budget.md) an
# 5 V aus USB. Das ist eine Spitze, kein Dauerwert.
I_SPITZE_A = 0.0675
U_USB_V = 5.0
# Waermeuebergang einer senkrechten, frei stehenden Kunststoffflaeche an
# ruhende Luft. ANNAHME aus der Literatur, nicht gemessen.
ALPHA_W_M2K = 10.0


def waerme(p=None, d=None):
    """Uebertemperatur der Gehaeuseoberflaeche bei Spitzenlast.

    Rechnung: P = U * I, A = Oberflaeche des Quaders, dT = P / (alpha * A).
    Der Verguss selbst verbessert die Waermeabfuhr, er verschlechtert sie
    nicht: gefuelltes Epoxid leitet rund 0,8 W/(m*K), Luft 0,026.
    """
    if p is None:
        p = geometry.values()
    if d is None:
        d = geometry.derive(p)
    b = d["outer_wid"] / 1000.0
    l = d["outer_len"] / 1000.0
    h = d["total_th"] / 1000.0
    flaeche = 2.0 * (b * l + b * h + l * h)
    leistung = U_USB_V * I_SPITZE_A
    return {
        "flaeche_m2": flaeche,
        "leistung_w": leistung,
        "dt_k": leistung / (ALPHA_W_M2K * flaeche),
    }


def bericht(p=None):
    """Druckt jede Lagensumme und jede Pruefung. Liefert die Ergebnisliste."""
    if p is None:
        p = geometry.values()
    zeilen = []
    ergebnisse = []
    for fall in faelle(p):
        r = rechne(p, fall)
        ergebnisse.append(r)
        zeilen.append("")
        zeilen.append("=== %s - %s" % (r["name"], r["erklaerung"]))
        for titel, liste, summenname in (("nach oben (+z)", r["oben"], "h_oben"),
                                         ("nach unten (-z)", r["unten"], "h_unten")):
            zeilen.append("  %s" % titel)
            for n, w, v in liste:
                zeilen.append("    %-24s %6.3f mm   %s" % (n, v, w))
            zeilen.append("    %-24s %6.3f mm   Summe ab Platinenmittelebene"
                          % ("Summe", r[summenname]))
        zeilen.append("  GEHAEUSEDICKE          %6.3f mm   Aussenflaeche unten bis oben"
                      % r["dicke"])
        for fn in PRUEFUNGEN:
            name, ok, text = fn(p, r)
            zeilen.append("    [%s] %-28s %s" % ("ok " if ok else "NEIN", name, text))
    w = waerme(p)
    zeilen.append("")
    zeilen.append("=== Waerme bei Spitzenlast")
    zeilen.append("    Leistung                 %6.3f W    %.1f mA aus docs/power-budget.md an %.0f V"
                  % (w["leistung_w"], I_SPITZE_A * 1000.0, U_USB_V))
    zeilen.append("    Oberflaeche              %6.1f cm2  Aussenflaeche des Quaders ohne Oese"
                  % (w["flaeche_m2"] * 10000.0))
    zeilen.append("    Uebertemperatur          %6.1f K    gerechnet mit alpha = %.0f W/(m2*K), ANNAHME"
                  % (w["dt_k"], ALPHA_W_M2K))
    zeilen.append("")
    zeilen.append("Zur Zeile 'Lagensumme unter 2,90 mm': die 2,90 mm sind die Hoehe der")
    zeilen.append("Steckerhuelle ueber alles, nicht die erlaubte Gehaeusedicke. Dass jeder")
    zeilen.append("wirkliche Fall dagegen anschlaegt, ist das Ergebnis, kein Fehler - warum,")
    zeilen.append("steht in masse.md Abschnitt 3.")
    print("\n".join(zeilen))
    return ergebnisse


# --- Selbstpruefung an Kontrollpunkten --------------------------------------

def kontrollpunkte(p):
    """Liefert [(Text, Pruefung, Ergebnis, soll_bestehen)] - beide Richtungen."""
    halb = p["pcb_th"] / 2.0
    alle = faelle(p)
    leer = rechne(p, alle[0])
    echt = rechne(p, [f for f in alle if f.name == "stufe1-b"][0])

    # Kuenstlicher Fall, der genau mittig steht: oben so hoch wie unten.
    # Dazu muss das Bauteil oben so hoch sein wie die freie Hoehe unten.
    frei_unten = echt["h_unten"] - halb - p["skin_t"]
    mittig = rechne(p, Fall("mittig-kuenstlich", frei_unten - p["pot_cover"], 0.0,
                            frei_unten - p["pot_cover"], p["pot_cover"], "kuenstlich"))

    kaputt = dict(echt)
    kaputt["oben"] = echt["oben"][:-1]          # Dach aus der Aufteilung entfernt

    hoch = rechne(p, Fall("zu-hohes-teil", p[GRENZE] + 0.1, 0.0, 0.0, 0.0, "kuenstlich"))

    return [
        ("Grenze: leeres Gehaeuse (%.2f mm) bleibt darunter" % leer["dicke"],
         pruefe_grenze, leer, True),
        ("Grenze: wirklicher Fall stufe1-b (%.2f mm) MUSS anschlagen" % echt["dicke"],
         pruefe_grenze, echt, False),
        ("Summenprobe: vollstaendige Aufteilung geht auf",
         pruefe_summe, echt, True),
        ("Summenprobe: fehlende Lage MUSS anschlagen",
         pruefe_summe, kaputt, False),
        ("Bauteilprobe: U1 mit %.2f mm ist erlaubt" % p["h_top_part"],
         pruefe_bauteil, echt, True),
        ("Bauteilprobe: Teil von %.2f mm MUSS anschlagen" % (p[GRENZE] + 0.1),
         pruefe_bauteil, hoch, False),
        ("Mittigkeit: kuenstlich symmetrischer Fall besteht",
         lambda p, r: pruefe_mittig(p, r, 0.05), mittig, True),
        ("Mittigkeit: Versatz %+.3f mm MUSS anschlagen" % hoch["versatz"],
         pruefe_mittig, hoch, False),
        ("Mittigkeit: wirklicher Fall (Versatz %+.3f mm) besteht" % echt["versatz"],
         pruefe_mittig, echt, True),
    ]


def selftest(p=None):
    """Prueft die Kontrollpunkte und liefert die Liste der Abweichungen."""
    if p is None:
        p = geometry.values()
    punkte = kontrollpunkte(p)
    schlecht = []
    for text, fn, r, soll in punkte:
        _name, ok, detail = fn(p, r)
        gut = (ok == soll)
        print("  [%s] %s  (%s)" % ("ok " if gut else "FEHLER", text, detail))
        if not gut:
            schlecht.append(text)
    print("  %d von %d Kontrollpunkten wie erwartet; %d davon mussten anschlagen."
          % (len(punkte) - len(schlecht), len(punkte),
             sum(1 for t in punkte if not t[3])))
    return schlecht


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--selftest" in argv:
        print("Kontrollpunkte der Hoehenrechnung:")
        schlecht = selftest()
        if schlecht:
            print("FEHLER: " + "; ".join(schlecht))
            return 1
        print("selftest: ok")
        return 0
    bericht()
    print("")
    print("Kontrollpunkte der Hoehenrechnung:")
    schlecht = selftest()
    if schlecht:
        print("FEHLER: " + "; ".join(schlecht))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
