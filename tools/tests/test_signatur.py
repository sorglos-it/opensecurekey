# -*- coding: utf-8 -*-
"""Pruefungen fuer den Signaturweg: Schluessel, die zwei Fassungen, Reproduzierbarkeit.

Was hier gemessen wird, laeuft am PC. NICHT gemessen - und darum steht es in
keinem dieser Tests - ist alles, was das nRF Connect SDK braucht: der ARM-Bau,
der Abbruch des CMake-Laufs ohne Schluessel, und ob imgtool das AAGUID-TLV
wirklich in den Abbildkopf schreibt. Siehe docs/signatur.md.
"""
import io
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
import build as build_mod
import common
import keyguard
import repro
import variant as variant_mod


class SchluesselTest(unittest.TestCase):
    """Der Schluessel darf nie im Projektordner liegen - und das muss auffallen."""

    def test_seed_allein_faellt_auf(self):
        self.assertTrue(keyguard.content_looks_like_key('a' * 64))
        self.assertTrue(keyguard.content_looks_like_key('a' * 64 + '\n'))

    def test_pem_am_anfang_faellt_auf(self):
        self.assertTrue(keyguard.content_looks_like_key('-----BEGIN PRIVATE KEY-----\nMII\n'))

    def test_pem_mitten_in_einer_quelldatei_geht_durch(self):
        # Gemessen am 30.10.2026: so hat der erste Entwurf auf drei eigene
        # Go-Pruefungen und eine C-Pruefung angeschlagen.
        self.assertFalse(keyguard.content_looks_like_key(
            'const key = "-----BEGIN PRIVATE KEY-----"\n'))

    def test_hex_innerhalb_einer_datei_geht_durch(self):
        # In tools/sign.py stehen die RFC-8032-Vektoren, und die sind 64
        # Zeichen lang. Eine Pruefung, die immer rot ist, sieht niemand an.
        self.assertFalse(keyguard.content_looks_like_key("V = ['%s']\n" % ('a' * 64)))

    def test_quelltext_faellt_nicht_auf_den_namen(self):
        for name in ('osk_ctap_key.c', 'osk_guard_keys.h', 'agent_key_test.go'):
            self.assertIsNone(keyguard.name_looks_like_key(name), name)

    def test_schluesselnamen_fallen_auf(self):
        for name in ('firmware-ed25519.key', 'mcuboot.pem', 'id_ed25519'):
            self.assertTrue(keyguard.name_looks_like_key(name), name)

    def test_oeffentlicher_teil_geht_durch(self):
        self.assertIsNone(keyguard.name_looks_like_key('firmware-ed25519.pub'))

    def test_der_projektordner_ist_sauber(self):
        self.assertEqual([], keyguard.scan(common.ROOT))

    def test_schluessel_im_arbeitsbaum_wird_gefunden(self):
        """Der Schutz wird zum ANSCHLAGEN gebracht, nicht nur beschrieben."""
        folder = tempfile.mkdtemp(prefix='osk-keyguard-')
        try:
            # Harmloser Name, den kein .gitignore-Muster kennt - genau der
            # Fall, fuer den es diese Schicht gibt.
            path = os.path.join(folder, 'notizen.txt')
            with io.open(path, 'w', encoding='utf-8') as handle:
                handle.write('b' * 64 + '\n')
            hits = keyguard.scan(folder)
            self.assertEqual(1, len(hits), hits)
            self.assertEqual('notizen.txt', hits[0][0])
        finally:
            for name in os.listdir(folder):
                os.remove(os.path.join(folder, name))
            os.rmdir(folder)


class McubootSchluesselTest(unittest.TestCase):
    """Ohne Schluessel kein Bau. Ohne ihn nimmt MCUboot seinen eigenen
    Beispielschluessel, und der ist oeffentlich bekannt."""

    def test_nicht_gesetzt_ist_ein_nein(self):
        path, why = build_mod.mcuboot_key({}, common.ROOT)
        self.assertIsNone(path)
        self.assertIn('OSK_MCUBOOT_KEY', why)

    def test_leer_ist_ein_nein(self):
        path, _ = build_mod.mcuboot_key({'OSK_MCUBOOT_KEY': '   '}, common.ROOT)
        self.assertIsNone(path)

    def test_im_projektordner_ist_ein_nein(self):
        inside = os.path.join(common.ROOT, 'mcuboot.pem')
        path, why = build_mod.mcuboot_key({'OSK_MCUBOOT_KEY': inside}, common.ROOT,
                                          isfile=lambda p: True)
        self.assertIsNone(path)
        self.assertIn('Projektordner', why)

    def test_nicht_vorhanden_ist_ein_nein(self):
        outside = os.path.join(tempfile.gettempdir(), 'osk-gibtsnicht.pem')
        path, why = build_mod.mcuboot_key({'OSK_MCUBOOT_KEY': outside}, common.ROOT,
                                          isfile=lambda p: False)
        self.assertIsNone(path)
        self.assertIn('nicht da', why)

    def test_der_pfad_steht_in_keiner_meldung(self):
        """Bauprotokolle wandern in Fehlerberichte - der Pfad verraet, wo der
        Schluessel liegt."""
        outside = os.path.join(tempfile.gettempdir(), 'ganz-geheimer-ordner', 'k.pem')
        _, why = build_mod.mcuboot_key({'OSK_MCUBOOT_KEY': outside}, common.ROOT,
                                       isfile=lambda p: False)
        self.assertNotIn('ganz-geheimer-ordner', why)

    def test_aussen_und_vorhanden_ist_ein_ja(self):
        outside = os.path.join(tempfile.gettempdir(), 'osk-attrappe.pem')
        path, why = build_mod.mcuboot_key({'OSK_MCUBOOT_KEY': outside}, common.ROOT,
                                          isfile=lambda p: True)
        self.assertIsNone(why)
        self.assertEqual(os.path.abspath(outside), path)

    def test_bau_ohne_schluessel_ist_FEHLER_und_nicht_uebersprungen(self):
        """'uebersprungen' waere hier das Schlimmste: dann faellt es erst auf
        dem Rechner auf, der wirklich baut."""
        result = build_mod.build_firmware('0.0.1.0', common.ROOT, environ={},
                                          runner=lambda *a, **k: (0, ''))
        self.assertEqual(common.FAILED, result.status)
        self.assertIn('Signaturschluessel', result.detail)


class FassungTest(unittest.TestCase):
    """Zwei Fassungen, zwei AAGUIDs - und beide Richtungen geprueft."""

    def test_die_tafel_hat_zwei_verschiedene_aaguids(self):
        table = variant_mod.read_aaguid_table(common.ROOT)
        self.assertEqual(2, len(table))
        self.assertNotEqual(table['rsa'], table['norsa'])
        for value in table.values():
            self.assertEqual(16, len(value))

    def test_beide_richtungen_im_dateinamen(self):
        for which in variant_mod.VARIANTS:
            name = variant_mod.name_for('0.0.1.0', 'nrf52840dk', which, '.bin')
            self.assertEqual(which, variant_mod.variant_from_name(name)[0], name)

    def test_norsa_wird_nicht_als_rsa_gelesen(self):
        """'norsa' enthaelt 'rsa'. Wer erst nach 'rsa' sucht, haelt jedes
        Abbild ohne RSA fuer eines mit."""
        self.assertEqual('norsa', variant_mod.variant_from_name('osk-x-norsa.bin')[0])

    def test_ohne_fassung_im_namen_keine_fassung(self):
        self.assertIsNone(variant_mod.variant_from_name('zephyr.bin')[0])

    def test_fehlende_zeile_ist_nicht_dasselbe_wie_n(self):
        """Wer die Zeile vergisst, bekommt still die Vorgabe - dann weiss
        niemand, ob die Fassung gewollt oder gestolpert ist."""
        self.assertIsNone(variant_mod.variant_from_config('CONFIG_LOG=y\n')[0])
        self.assertEqual('norsa', variant_mod.variant_from_config('CONFIG_OSK_RSA=n\n')[0])
        self.assertEqual('rsa', variant_mod.variant_from_config('CONFIG_OSK_RSA=y\n')[0])

    def test_kconfig_schreibweise_eines_n(self):
        self.assertEqual('norsa',
                         variant_mod.variant_from_config('# CONFIG_OSK_RSA is not set\n')[0])

    def test_beide_baudateien_sagen_ausdruecklich_welche_fassung(self):
        source = common.part_dir('firmware', common.ROOT)
        for which in variant_mod.VARIANTS:
            path = os.path.join(source, 'variant-%s.conf' % which)
            self.assertTrue(os.path.isfile(path), path)
            with io.open(path, encoding='utf-8') as handle:
                self.assertEqual(which, variant_mod.variant_from_config(handle.read())[0], path)

    def test_kconfig_erklaert_die_option(self):
        """Ohne diese Datei kennt Kconfig den Namen CONFIG_OSK_RSA nicht, und
        jedes 'CONFIG_OSK_RSA=y' wird still verworfen."""
        path = os.path.join(common.part_dir('firmware', common.ROOT), 'Kconfig')
        self.assertTrue(os.path.isfile(path), path)
        with io.open(path, encoding='utf-8') as handle:
            self.assertIn('config OSK_RSA', handle.read())

    def test_aaguid_aus_dem_abbildkopf_beide_fassungen(self):
        table = variant_mod.read_aaguid_table(common.ROOT)
        for which in variant_mod.VARIANTS:
            data = variant_mod.build_image(b'\x11' * 64, which, root=common.ROOT)
            self.assertEqual(table[which], variant_mod.aaguid_from_image(data)[0], which)

    def test_abbild_ohne_mcuboot_kopf_faellt_auf(self):
        with self.assertRaises(variant_mod.VariantError):
            variant_mod.read_image_header(b'\x00' * 64)

    def test_ungeschuetztes_aaguid_tlv_faellt_auf(self):
        """Im ungeschuetzten Bereich ist der AAGUID austauschbar, ohne die
        Signatur zu brechen - und ein austauschbarer AAGUID ist keiner."""
        data = variant_mod.build_image(b'\x11' * 64, 'rsa', root=common.ROOT, protected=False)
        with self.assertRaises(variant_mod.VariantError):
            variant_mod.aaguid_from_image(data)


class VerwechslungTest(unittest.TestCase):
    """Der gefaehrlichste Fall: ein Abbild OHNE RSA mit dem AAGUID der Fassung
    MIT RSA. Dann verspricht das Geraet Faehigkeiten, die es nicht hat."""

    def setUp(self):
        self.folder = tempfile.mkdtemp(prefix='osk-fassung-')
        self.table = variant_mod.read_aaguid_table(common.ROOT)

    def tearDown(self):
        for name in os.listdir(self.folder):
            os.remove(os.path.join(self.folder, name))
        os.rmdir(self.folder)

    def write(self, name, which, aaguid=None):
        path = os.path.join(self.folder, name)
        with io.open(path, 'wb') as handle:
            handle.write(variant_mod.build_image(b'\xaa' * 256, which, root=common.ROOT,
                                                 aaguid=aaguid))
        return path

    def test_norsa_mit_rsa_aaguid_wird_abgefangen(self):
        path = self.write('osk-firmware-0.0.1.0-nrf52840dk-norsa.bin', 'norsa',
                          aaguid=self.table['rsa'])
        good, lines = variant_mod.check(path, root=common.ROOT)
        self.assertFalse(good)
        self.assertTrue(any('GEFAEHRLICHE' in l for l in lines), lines)

    def test_rsa_mit_norsa_aaguid_wird_auch_abgefangen(self):
        """Die andere Richtung ist weniger gefaehrlich, aber auch falsch -
        eine Option, die nur in einer Richtung geprueft ist, ist keine."""
        path = self.write('osk-firmware-0.0.1.0-nrf52840dk-rsa.bin', 'rsa',
                          aaguid=self.table['norsa'])
        good, _ = variant_mod.check(path, root=common.ROOT)
        self.assertFalse(good)

    def test_erwartung_gegen_abbild(self):
        path = self.write('osk-firmware-0.0.1.0-nrf52840dk-rsa.bin', 'rsa')
        self.assertTrue(variant_mod.check(path, expect='rsa', root=common.ROOT)[0])
        self.assertFalse(variant_mod.check(path, expect='norsa', root=common.ROOT)[0])

    def test_fremder_aaguid_wird_abgelehnt(self):
        path = self.write('osk-firmware-0.0.1.0-nrf52840dk-rsa.bin', 'rsa',
                          aaguid=b'\x00' * 16)
        good, lines = variant_mod.check(path, root=common.ROOT)
        self.assertFalse(good)
        self.assertTrue(any('keiner der beiden Fassungen' in l for l in lines), lines)

    def test_abbild_ohne_fassung_im_namen_wird_abgelehnt(self):
        path = self.write('zephyr.bin', 'rsa')
        self.assertFalse(variant_mod.check(path, root=common.ROOT)[0])

    def test_beide_fassungen_richtig_gehen_durch(self):
        """Die Gegenprobe. Eine Pruefung, die alles ablehnt, misst nichts."""
        for which in variant_mod.VARIANTS:
            path = self.write('osk-firmware-0.0.1.0-nrf52840dk-%s.bin' % which, which)
            good, lines = variant_mod.check(path, expect=which, root=common.ROOT)
            self.assertTrue(good, lines)


class ReproTest(unittest.TestCase):
    """Zweimal verpacken muss dieselben Bytes ergeben."""

    def test_paket_und_abbildkopf_sind_reproduzierbar(self):
        ok, lines = repro.check_pack(common.ROOT)
        self.assertTrue(ok, '\n'.join(lines))

    def test_git_hash_object_wird_benutzt(self):
        """Nicht eine eigene Hashrechnung: dieselbe Zahl, die git in jedem
        Commit fuehrt, von Hand nachrechenbar."""
        path = os.path.join(TOOLS, 'variant.py')
        value, why = repro.git_hash(path)
        self.assertIsNone(why)
        self.assertEqual(40, len(value))

    def test_eine_abweichung_wird_benannt(self):
        folder = tempfile.mkdtemp(prefix='osk-repro-test-')
        try:
            left = os.path.join(folder, 'a.bin')
            right = os.path.join(folder, 'b.bin')
            with io.open(left, 'wb') as handle:
                handle.write(b'gleich' + b'\x01' + b'rest')
            with io.open(right, 'wb') as handle:
                handle.write(b'gleich' + b'\x02' + b'rest')
            same, lines = repro.compare('Probe', left, right)
            self.assertFalse(same)
            self.assertTrue(any('Byte 6' in l for l in lines), lines)
        finally:
            for name in os.listdir(folder):
                os.remove(os.path.join(folder, name))
            os.rmdir(folder)


if __name__ == '__main__':
    unittest.main()
