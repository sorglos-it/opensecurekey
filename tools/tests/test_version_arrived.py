# -*- coding: utf-8 -*-
"""Tests fuer die Messung "kam die Version im Abbild an?" in build.py.

Der Fall, den sie abdeckt: bei einem Sysbuild laeuft cmake zweimal, und alles
hinter '--' landet nur im aeusseren Lauf. Gemessen am 25.10.2026 mit zwei
geschachtelten cmake-Laeufen - '-DOSK_VERSION=' und '-Dfirmware_OSK_VERSION='
stehen danach nur im aeusseren Zwischenspeicher, die Umgebungsvariable
OSK_VERSION kommt im inneren Lauf an. Die Zwischenspeicher hier sind
nachgebaut, damit die Tests ohne nRF Connect SDK laufen.
"""
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import build
import common
from common import OK, SKIPPED, FAILED


def cache(folder, *lines):
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, 'CMakeCache.txt')
    with io.open(path, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write('# This is the CMakeCache file.\n' + ''.join(l + '\n' for l in lines))
    return path


class Zwischenspeicher(unittest.TestCase):
    def test_wert_wird_gelesen(self):
        folder = tempfile.mkdtemp()
        path = cache(folder, 'OSK_VERSION:STRING=1.2.3.4', 'ANDERES:BOOL=ON')
        self.assertEqual(build.cache_value(path, 'OSK_VERSION'), '1.2.3.4')
        self.assertIsNone(build.cache_value(path, 'FEHLT'))

    def test_aeusserer_zuerst(self):
        root = tempfile.mkdtemp()
        cache(os.path.join(root, 'firmware'), 'X:BOOL=ON')
        cache(root, 'X:BOOL=ON')
        found = build.cmake_caches(root)
        self.assertEqual([os.path.relpath(p, root) for p in found],
                         ['CMakeCache.txt', os.path.join('firmware', 'CMakeCache.txt')])


class Angekommen(unittest.TestCase):
    def test_sysbuild_nur_aussen_ist_ein_fehler(self):
        """Genau der gemeldete Fall: mess-build/CMakeCache.txt hat die
        Variable, mess-build/firmware/CMakeCache.txt nicht."""
        root = tempfile.mkdtemp()
        cache(root, 'OSK_VERSION:UNINITIALIZED=0.0.1.0')
        cache(os.path.join(root, 'firmware'), 'CMAKE_BUILD_TYPE:STRING=Debug')
        ok, text = build.version_arrived(root, '0.0.1.0')
        self.assertFalse(ok)
        self.assertIn('firmware', text)

    def test_sysbuild_mit_version_im_abbild_ist_gut(self):
        root = tempfile.mkdtemp()
        cache(root, 'OSK_VERSION:UNINITIALIZED=0.0.1.0')
        cache(os.path.join(root, 'firmware'), 'OSK_VERSION:STRING=0.0.1.0')
        ok, text = build.version_arrived(root, '0.0.1.0')
        self.assertTrue(ok)
        self.assertIn('0.0.1.0', text)

    def test_falsche_version_im_abbild_ist_ein_fehler(self):
        root = tempfile.mkdtemp()
        cache(root, 'OSK_VERSION:UNINITIALIZED=0.0.2.0')
        cache(os.path.join(root, 'firmware'), 'OSK_VERSION:STRING=0.0.1.0')
        self.assertFalse(build.version_arrived(root, '0.0.2.0')[0])

    def test_ohne_sysbuild_zaehlt_der_einzige_lauf(self):
        root = tempfile.mkdtemp()
        cache(root, 'OSK_VERSION:UNINITIALIZED=0.0.1.0')
        self.assertTrue(build.version_arrived(root, '0.0.1.0')[0])
        self.assertFalse(build.version_arrived(root, '9.9.9.9')[0])

    def test_kein_zwischenspeicher_ist_ein_fehler(self):
        ok, text = build.version_arrived(tempfile.mkdtemp(), '0.0.1.0')
        self.assertFalse(ok)
        self.assertIn('CMakeCache.txt', text)


class Aufruf(unittest.TestCase):
    """Wird die Version ueberhaupt auf allen drei Wegen mitgegeben?"""

    def setUp(self):
        self.root = tempfile.mkdtemp()
        for part in common.PARTS:
            path = os.path.join(self.root, 'apps', part)
            os.makedirs(path, exist_ok=True)
            with io.open(os.path.join(path, 'VERSION'), 'w', encoding='utf-8', newline='\n') as h:
                h.write('0.0.1.0\n')
        with io.open(os.path.join(self.root, 'apps', 'firmware', 'CMakeLists.txt'),
                     'w', encoding='utf-8', newline='\n') as handle:
            handle.write('# Platzhalter\n')
        self.calls = []
        # 'west' gibt es auf diesem Rechner nicht - fuer diesen Test wird nur
        # geprueft, WAS build.py uebergibt, nicht ob ein Bau zustande kommt.
        self.echtes_which = common.which
        common.which = lambda name: '/fake/' + name

    def tearDown(self):
        common.which = self.echtes_which

    def runner(self, args, cwd=None, env=None, capture=False, windows=None):
        self.calls.append((list(args), env))
        return 0, ''

    def test_umgebungsvariable_und_beide_schreibweisen(self):
        build.build_firmware('0.0.1.0', self.root,
                             environ={'PATH': 'x', 'ZEPHYR_BASE': '/zephyr'},
                             runner=self.runner)
        self.assertEqual(len(self.calls), 1, 'west wurde nicht aufgerufen')
        args, env = self.calls[0]
        self.assertIn('-DOSK_VERSION=0.0.1.0', args)
        self.assertIn('-Dfirmware_OSK_VERSION=0.0.1.0', args)
        self.assertEqual((env or {}).get('OSK_VERSION'), '0.0.1.0')

    def test_ohne_abbild_bleibt_es_ein_fehler(self):
        """Der Laeufer meldet Erfolg, im Bauordner liegt aber nichts - das
        darf nie 'ok' werden."""
        result = build.build_firmware('0.0.1.0', self.root,
                                      environ={'PATH': 'x', 'ZEPHYR_BASE': '/zephyr'},
                                      runner=self.runner)
        self.assertEqual(result.status, FAILED)


if __name__ == '__main__':
    unittest.main()
