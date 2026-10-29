# -*- coding: utf-8 -*-
"""Gegenproben zu den Pruefungen des Gehaeuses.

    cd cad/tools && python -m unittest -v test_cad

Die Pruefungen in budget.py und build_mesh.py sagen bei diesem Entwurf 'ok'.
Das allein beweist nichts: eine Pruefung, die nie angeschlagen hat, ist keine.
Darum macht diese Datei den Entwurf absichtlich kaputt und verlangt, dass die
Pruefung es MERKT.
"""

import unittest

import budget
import build_mesh
import geometry


class Kontrollpunkte(unittest.TestCase):

    def setUp(self):
        self.p = geometry.values()
        self.d = geometry.derive(self.p)

    def test_hoehenrechnung_kontrollpunkte(self):
        """Alle Kontrollpunkte der Hoehenrechnung verhalten sich wie erwartet."""
        self.assertEqual(budget.selftest(self.p), [])

    def test_mindestens_eine_pruefung_schlaegt_an(self):
        """Von den Kontrollpunkten muessen welche anschlagen, nicht alle bestehen."""
        punkte = budget.kontrollpunkte(self.p)
        self.assertTrue(any(not soll for _t, _f, _r, soll in punkte))
        self.assertTrue(any(soll for _t, _f, _r, soll in punkte))

    def test_koerper_proben_bestehen(self):
        anzahl, voll = build_mesh.check_openings(self.p, self.d)
        self.assertGreater(anzahl, 20)
        self.assertGreater(voll, 0)
        self.assertGreater(anzahl - voll, 0)

    def test_fehlender_tasterschlitz_wird_gemerkt(self):
        """Nimmt man SW1 den Schlitz, muss die Probe anschlagen."""
        echt = geometry.shell_top

        y0 = self.p["sw1_cy"] - self.p["btn_slot_y"] / 2.0
        y1 = self.p["sw1_cy"] + self.p["btn_slot_y"] / 2.0

        def ohne_schlitz(p, d, preview=True):
            keep = []
            for sign, box in echt(p, d, preview):
                ist_schlitz = (sign == "-" and abs(box[0] - d["ot_x0"]) < 1e-9
                               and abs(box[1] - d["pk_x0"]) < 1e-9
                               and abs(box[2] - y0) < 1e-9 and abs(box[3] - y1) < 1e-9)
                if not ist_schlitz:
                    keep.append((sign, box))
            return keep

        entfernt = len(echt(self.p, self.d)) - len(ohne_schlitz(self.p, self.d))
        self.assertEqual(entfernt, 1, "der Schlitz wurde nicht gefunden")
        geometry.shell_top = ohne_schlitz
        try:
            with self.assertRaises(ValueError):
                build_mesh.check_openings(self.p, self.d)
        finally:
            geometry.shell_top = echt

    def test_fehlender_damm_wird_gemerkt(self):
        """Nimmt man dem microSD-Schacht den Damm, muss die Probe anschlagen."""
        echt = geometry.islands_bottom

        def ohne_damm(p, d):
            return [(n, box, [], 0.0) for n, box, _s, _t in echt(p, d)]

        geometry.islands_bottom = ohne_damm
        try:
            with self.assertRaises(ValueError):
                build_mesh.check_openings(self.p, self.d)
        finally:
            geometry.islands_bottom = echt

    def test_zu_dickes_gehaeuse_wird_gemerkt(self):
        """Ein Bauteil hoeher als die Steckerhuelle muss die Bauteilprobe reissen."""
        p = dict(self.p)
        p["h_top_part"] = p["usb_shell_h"] + 0.5
        r = budget.rechne(p, budget.faelle(p)[1])
        _name, ok, _text = budget.pruefe_bauteil(p, r)
        self.assertFalse(ok)

    def test_parameter_haben_alle_einen_kommentar(self):
        """Jede Zahl muss sagen, WAS sie misst - sonst ist sie unbrauchbar."""
        ohne = [n for n, (_v, c) in geometry.read_parameters().items() if len(c) < 10]
        self.assertEqual(ohne, [])


if __name__ == "__main__":
    unittest.main()
