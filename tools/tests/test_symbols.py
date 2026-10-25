# -*- coding: utf-8 -*-
"""Tests fuer symbols.py und den Schritt firmware/symbole in check.py.

Jede Pruefung hier legt den Fehler erst an, den sie finden will - eine
Kollisionspruefung, die man nicht zum Anschlagen gebracht hat, ist keine.
"""
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import check
import common
import symbols
from common import OK, SKIPPED, FAILED


def write(root, rel, text):
    path = os.path.join(root, *rel.split('/'))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with io.open(path, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(text)
    return path


def firmware(**files):
    """Wegwerf-Repo mit apps/firmware/src/classes/<teil>/<datei>."""
    root = tempfile.mkdtemp()
    for part in common.PARTS:
        write(root, 'apps/%s/VERSION' % part, '0.0.0.0\n')
    for rel, text in files.items():
        write(root, 'apps/firmware/' + rel, text)
    return root


class Lesen(unittest.TestCase):
    def test_nicht_statische_funktion_zaehlt(self):
        found = symbols.definitions('int osk_tu(int a)\n{\n\treturn a;\n}\n')
        self.assertEqual([(n, k) for n, _, k, _ in found], [('osk_tu', 'funktion')])

    def test_static_zaehlt_nicht(self):
        self.assertEqual(symbols.definitions('static int hilf(void)\n{\n\treturn 0;\n}\n'), [])

    def test_prototyp_ist_keine_definition(self):
        self.assertEqual(symbols.definitions('int osk_tu(int a);\n'), [])

    def test_globale_variable_zaehlt(self):
        found = symbols.definitions('int osk_zaehler = 3;\n')
        self.assertEqual([(n, k) for n, _, k, _ in found], [('osk_zaehler', 'variable')])

    def test_extern_zaehlt_nicht(self):
        self.assertEqual(symbols.definitions('extern int osk_zaehler;\n'), [])

    def test_name_im_rumpf_zaehlt_nicht(self):
        """Ein Aufruf in einer Funktion ist keine Definition."""
        found = symbols.definitions('void osk_a(void)\n{\n\tosk_b(1);\n\tint x = 2;\n}\n')
        self.assertEqual([n for n, _, _, _ in found], ['osk_a'])

    def test_kommentar_und_zeichenkette_taeuschen_nicht(self):
        text = '/* void osk_falsch(void) { } */\nconst char *s = "void osk_auch(void) {";\n'
        self.assertEqual([n for n, _, _, _ in symbols.definitions(text)], ['s'])

    def test_zeilennummer_stimmt(self):
        text = '\n\n\nvoid osk_tu(void)\n{\n}\n'
        self.assertEqual([(n, line) for n, line, _, _ in symbols.definitions(text)], [('osk_tu', 4)])

    def test_if_wird_als_bedingt_gemerkt(self):
        text = '#if defined(X)\nvoid osk_tu(void)\n{\n}\n#endif\n'
        self.assertEqual([b for _, _, _, b in symbols.definitions(text)], [True])

    def test_doppelaufnahme_riegel_gilt_nicht_als_bedingung(self):
        """'#ifndef X_H' + '#define X_H' ist ein Riegel, keine Bedingung -
        sonst waere keine Ankuendigung in keinem Kopf je zu pruefen."""
        text = '#ifndef OSK_X_H\n#define OSK_X_H\nint osk_tu(int a);\n#endif\n'
        self.assertEqual([(n, k, b) for n, _, k, b in symbols.prototypes(text)],
                         [('osk_tu', 'int', False)])


class Kollisionen(unittest.TestCase):
    def scan(self, **files):
        root = firmware(**files)
        return symbols.scan(os.path.join(root, 'apps', 'firmware'))

    def test_zwei_teile_gleicher_name_ist_ein_fehler(self):
        doubles, clashes, hints, _, _ = self.scan(**{
            'src/classes/ctap/a.c': 'void osk_wipe(void *p)\n{\n}\n',
            'src/classes/se050/b.c': 'void osk_wipe(void *p)\n{\n}\n'})
        self.assertEqual([n for n, _, _ in doubles], ['osk_wipe'])
        self.assertEqual(doubles[0][2], ['ctap/a.c:1', 'se050/b.c:1'])
        self.assertEqual(hints, [])

    def test_ein_name_in_einer_datei_ist_kein_fehler(self):
        doubles, clashes, hints, files, names = self.scan(**{
            'src/classes/ctap/a.c': 'void osk_wipe(void *p)\n{\n}\n'})
        self.assertEqual((doubles, clashes, hints), ([], [], []))
        self.assertEqual((files, names), (1, 1))

    def test_verschiedene_rueckgabetypen_sind_ein_fehler(self):
        """Der Fall osk_apdu_parse: osk_result_t gegen bool. Weil OSK_OK == 0
        ist, kehrt das Erfolg und Fehler um - und genau das stand im Abbild."""
        doubles, clashes, hints, _, _ = self.scan(**{
            'src/classes/applets/osk_apdu.h': 'osk_result_t osk_apdu_parse(const void *b);\n',
            'src/classes/transport/nfc_apdu.h': 'bool osk_apdu_parse(const void *b);\n'})
        self.assertEqual([n for n, _ in clashes], ['osk_apdu_parse'])
        self.assertEqual(sorted(k for _, k in clashes[0][1]), ['bool', 'osk_result_t'])

    def test_bedingte_doppelung_ist_nur_ein_hinweis(self):
        """Drei Krypto-Unterbauten tragen dieselben Namen und schliessen sich
        per #if aus - das darf den Pruefsatz nicht rot machen."""
        a = '#if defined(_WIN32)\nvoid osk_aes_cmac(void)\n{\n}\n#endif\n'
        b = '#if defined(__unix__)\nvoid osk_aes_cmac(void)\n{\n}\n#endif\n'
        doubles, clashes, hints, _, _ = self.scan(**{
            'src/classes/se050/cng.c': a, 'src/classes/se050/openssl.c': b})
        self.assertEqual(doubles, [])
        self.assertEqual([n for n, _ in hints], ['osk_aes_cmac'])

    def test_testordner_wird_nicht_mitgelesen(self):
        """Die Pruefsaetze bauen eigene Doppel - sie kommen nicht ins Abbild."""
        doubles, _, _, files, _ = self.scan(**{
            'src/classes/ctap/a.c': 'void osk_wipe(void *p)\n{\n}\n',
            'test/ctap/fake.c': 'void osk_wipe(void *p)\n{\n}\n'})
        self.assertEqual(doubles, [])
        self.assertEqual(files, 1)


class Schritt(unittest.TestCase):
    def test_schritt_wird_rot_und_nennt_beide_stellen(self):
        root = firmware(**{'src/classes/ctap/a.c': 'void osk_wipe(void *p)\n{\n}\n',
                           'src/classes/se050/b.c': 'void osk_wipe(void *p)\n{\n}\n'})
        result = check.check_firmware_symbols(root)
        self.assertEqual((result.part, result.step, result.status), ('firmware', 'symbole', FAILED))
        self.assertIn('ctap/a.c:1', result.detail)
        self.assertIn('se050/b.c:1', result.detail)

    def test_schritt_wird_gruen_ohne_kollision(self):
        root = firmware(**{'src/classes/ctap/a.c': 'void osk_wipe(void *p)\n{\n}\n',
                           'src/classes/se050/b.c': 'void se050_wipe(void *p)\n{\n}\n'})
        result = check.check_firmware_symbols(root)
        self.assertEqual(result.status, OK)
        self.assertIn('2 Dateien', result.detail)

    def test_schritt_ohne_quellen_wird_uebersprungen(self):
        root = firmware()
        self.assertEqual(check.check_firmware_symbols(root).status, SKIPPED)

    def test_schritt_ohne_c_dateien_wird_uebersprungen(self):
        root = firmware(**{'src/classes/ctap/a.h': 'int osk_tu(int a);\n'})
        result = check.check_firmware_symbols(root)
        self.assertEqual(result.status, SKIPPED)
        self.assertIn('keine .c', result.detail)

    def test_schritt_steht_im_gesamtlauf(self):
        """Ein Schritt, den run_all nicht aufruft, prueft nichts."""
        root = firmware(**{'src/classes/ctap/a.c': 'void osk_tu(void)\n{\n}\n'})
        steps = [(r.part, r.step) for r in check.run_all(root)]
        self.assertIn(('firmware', 'symbole'), steps)


if __name__ == '__main__':
    unittest.main()


class BlindeFlecken(unittest.TestCase):
    """Was die Pruefung NICHT findet - festgenagelt, damit es im Kopf von
    symbols.py nicht irgendwann eine Behauptung ohne Messung ist."""

    def scan(self, **files):
        root = firmware(**files)
        return symbols.scan(os.path.join(root, 'apps', 'firmware'))

    def test_kollision_mit_fremdem_code_faellt_nicht_auf(self):
        doubles, clashes, _, _, _ = self.scan(**{
            'src/classes/ctap/a.c': 'void *memcpy(void *a, const void *b, unsigned long n)\n{\n\treturn a;\n}\n'})
        self.assertEqual((doubles, clashes), ([], []))

    def test_name_aus_einem_makro_faellt_nicht_auf(self):
        text = '#define MACHE(n) void osk_##n(void) { }\nMACHE(tu)\n'
        doubles, _, hints, _, _ = self.scan(**{'src/classes/ctap/a.c': text,
                                               'src/classes/store/b.c': text})
        self.assertEqual((doubles, hints), ([], []))

    def test_zweimal_in_einer_datei_faellt_nicht_auf(self):
        doubles, _, _, _, _ = self.scan(**{
            'src/classes/ctap/a.c': 'void osk_x(void)\n{\n}\nvoid osk_x(void)\n{\n}\n'})
        self.assertEqual(doubles, [])

    def test_ausgeknipster_code_wird_trotzdem_gelesen(self):
        """'#if 0' nimmt die Definition aus dem Bau - gemeldet wird sie als
        Hinweis, nicht als Fehler."""
        doubles, _, hints, _, _ = self.scan(**{
            'src/classes/ctap/a.c': '#if 0\nvoid osk_tu(void)\n{\n}\n#endif\n',
            'src/classes/store/b.c': 'void osk_tu(void)\n{\n}\n'})
        self.assertEqual(doubles, [])
        self.assertEqual([n for n, _ in hints], ['osk_tu'])


class Zusaetze(unittest.TestCase):
    def scan(self, **files):
        root = firmware(**files)
        return symbols.scan(os.path.join(root, 'apps', 'firmware'))

    def test_attribute_verdeckt_die_definition_nicht(self):
        """'__attribute__((aligned(4))) void osk_tu(void)' sah vorher aus wie
        ein Aufruf von __attribute__ - die Definition wurde gar nicht gelesen."""
        doubles, _, _, _, names = self.scan(**{
            'src/classes/ctap/a.c': '__attribute__((aligned(4))) void osk_tu(void)\n{\n}\n',
            'src/classes/store/b.c': 'void osk_tu(void)\n{\n}\n'})
        self.assertEqual([n for n, _, _ in doubles], ['osk_tu'])

    def test_schwaches_symbol_ist_nur_ein_hinweis(self):
        """Ein weak-Symbol darf ein starkes doppeln: der Binder nimmt das starke."""
        doubles, _, hints, _, _ = self.scan(**{
            'src/classes/ctap/a.c': '__attribute__((weak)) void osk_tu(void)\n{\n}\n',
            'src/classes/store/b.c': 'void osk_tu(void)\n{\n}\n'})
        self.assertEqual(doubles, [])
        self.assertEqual([n for n, _ in hints], ['osk_tu'])

    def test_weak_kurzform(self):
        self.assertTrue(symbols.is_weak('__weak void osk_tu(void)'))
        self.assertFalse(symbols.is_weak('void osk_tu(void)'))
