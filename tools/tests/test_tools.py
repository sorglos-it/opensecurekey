# -*- coding: utf-8 -*-
"""Tests fuer build, check, release und flash - mit kleinen Wegwerf-Repos."""
import io
import json
import os
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import build
import check
import common
import flash
import release
from common import OK, SKIPPED, FAILED, Result


def write(root, rel, text):
    path = os.path.join(root, *rel.split('/'))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with io.open(path, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(text)
    return path


# Die beiden AAGUIDs stehen im echten Baum in osk_ctap_core.c, und tools/
# liest sie DORT (eine Zahl nur an einer Stelle). Ein Wegwerf-Repo hat die
# Datei nicht, und ein Bau ohne AAGUID bricht zu Recht ab - er koennte die
# Fassung nicht in den Abbildkopf schreiben. Darum bekommt jedes Wegwerf-Repo
# eine kleine Fassung davon. Die Werte sind hier egal; wichtig ist, dass es
# zwei verschiedene sind.
AAGUID_STUB = '''static const uint8_t OSK_AAGUID_RSA[16] = {
    0x6f, 0x73, 0x6b, 0x2d, 0x6f, 0x70, 0x65, 0x6e, 0x73, 0x65, 0x63, 0x6b, 0x65, 0x79, 0x00, 0x01,
};
static const uint8_t OSK_AAGUID_NO_RSA[16] = {
    0x6f, 0x73, 0x6b, 0x2d, 0x6f, 0x70, 0x65, 0x6e, 0x73, 0x65, 0x63, 0x6b, 0x65, 0x79, 0x00, 0x02,
};
'''


def write_aaguid_table(root):
    return write(root, 'apps/firmware/src/classes/ctap/osk_ctap_core.c', AAGUID_STUB)


def repo(version='0.0.0.0'):
    root = tempfile.mkdtemp()
    for part in common.PARTS:
        write(root, 'apps/%s/VERSION' % part, version + '\n')
    return root


class Summary(unittest.TestCase):
    def test_zero_tests_is_never_success(self):
        code, text = check.summarize([Result('tools', 'x', OK, 'gelaufen', 0)])
        self.assertEqual(code, 1)
        self.assertIn('kein einziger Test', text)

    def test_failed_step_is_exit_1_even_with_tests(self):
        code, _ = check.summarize([Result('a', 'tests', OK, '', 5), Result('b', 'build', FAILED, 'kaputt')])
        self.assertEqual(code, 1)

    def test_skipped_is_named_and_not_hidden(self):
        code, text = check.summarize([Result('a', 'tests', OK, '', 2), Result('firmware', 'build', SKIPPED, 'kein SDK')])
        self.assertEqual(code, 0)
        self.assertIn('NICHT geprueft', text)
        self.assertIn('firmware/build', text)


class EmptyParts(unittest.TestCase):
    def test_empty_repo_skips_everything_honestly(self):
        root = repo()
        for part in common.PARTS:
            self.assertFalse(common.has_sources(part, root))
        self.assertEqual({r.status for r in check.check_desktop(root)}, {SKIPPED})
        self.assertEqual({r.status for r in check.check_web(root)}, {SKIPPED})
        self.assertEqual(build.build('desktop', root).status, SKIPPED)
        self.assertEqual(build.build('web', root).status, SKIPPED)
        self.assertEqual(build.build('firmware', root).status, SKIPPED)
        self.assertEqual(build.main(['all'], root), 2)     # nichts gebaut: nie Code 0


class FirmwareSdk(unittest.TestCase):
    def test_missing_sdk_is_reported_and_aborts_cleanly(self):
        root = repo()
        write(root, 'apps/firmware/CMakeLists.txt', 'project(osk)\n')
        # Seit dem 30.10.2026 wird der Signaturschluessel VOR dem SDK geprueft
        # (ein fehlender Schluessel ist schlimmer als ein fehlendes SDK, siehe
        # build.mcuboot_key). Damit dieser Test weiter das SDK misst und nicht
        # den Schluessel, bekommt er beides mit: die Baudatei der Fassung und
        # einen Schluesselpfad AUSSERHALB dieses Baums.
        write(root, 'apps/firmware/variant-norsa.conf', 'CONFIG_OSK_RSA=n\n')
        write_aaguid_table(root)
        key = os.path.join(tempfile.gettempdir(), 'osk-test-attrappe.pem')
        with io.open(key, 'w', encoding='utf-8') as handle:
            handle.write('kein echter Schluessel\n')
        called = []
        result = build.build_firmware('0.0.0.0', root, environ={'OSK_MCUBOOT_KEY': key},
                                      runner=lambda *a, **k: called.append(a))
        self.assertEqual(result.status, SKIPPED)
        self.assertIn('nRF Connect SDK nicht gefunden', result.detail)
        self.assertEqual(called, [])

    def test_missing_key_is_a_failure_before_the_sdk_is_even_looked_for(self):
        """Ohne Schluessel nimmt MCUboot seinen oeffentlich bekannten
        Beispielschluessel. 'uebersprungen' waere hier das Schlimmste: dann
        faellt es erst auf dem Rechner auf, der wirklich baut."""
        root = repo()
        write(root, 'apps/firmware/CMakeLists.txt', 'project(osk)\n')
        write(root, 'apps/firmware/variant-norsa.conf', 'CONFIG_OSK_RSA=n\n')
        called = []
        result = build.build_firmware('0.0.0.0', root, environ={},
                                      runner=lambda *a, **k: called.append(a))
        self.assertEqual(result.status, FAILED)
        self.assertIn('Signaturschluessel', result.detail)
        self.assertEqual(called, [])

    def test_sdk_through_nrfutil_needs_version(self):
        have = lambda name: name if name in ('nrfutil', 'west') else None
        self.assertIsNone(build.find_sdk({}, which=have)[0])
        prefix, _ = build.find_sdk({'OSK_NCS': 'v3.0.0'}, which=have)
        self.assertEqual(prefix[:3], ['nrfutil', 'toolchain-manager', 'launch'])
        prefix, _ = build.find_sdk({'ZEPHYR_BASE': 'x'}, which=have)
        self.assertEqual(prefix, ['west'])

    def test_failed_west_is_a_failure_not_a_success(self):
        root = repo()
        write(root, 'apps/firmware/CMakeLists.txt', 'project(osk)\n')
        result = build.build_firmware('0.0.0.1', root, environ={'ZEPHYR_BASE': 'x'},
                                      runner=lambda *a, **k: (1, ''))
        # west kann im Testrechner fehlen -> dann uebersprungen; nie ok
        self.assertNotEqual(result.status, OK)


class Web(unittest.TestCase):
    def test_manifest_gets_version_only_in_the_zip(self):
        root = repo('0.1.2.3')
        write(root, 'apps/web/manifest.json', json.dumps({'name': 'osk', 'version': '9.9.9'}))
        write(root, 'apps/web/background.js', 'let a = 1;\n')
        write(root, 'apps/web/tests/a.test.js', 'x\n')
        result = build.build_web('0.1.2.3', root)
        self.assertEqual(result.status, OK, result.detail)
        archive = zipfile.ZipFile(os.path.join(root, 'dist', 'osk-web-0.1.2.3.zip'))
        self.assertEqual(json.loads(archive.read('manifest.json'))['version'], '0.1.2.3')
        self.assertEqual(sorted(archive.namelist()), ['background.js', 'manifest.json'])
        with io.open(os.path.join(root, 'apps/web/manifest.json'), encoding='utf-8') as handle:
            self.assertEqual(json.load(handle)['version'], '9.9.9')

    def test_broken_manifest_fails(self):
        root = repo()
        write(root, 'apps/web/manifest.json', '{kaputt')
        self.assertEqual(build.build_web('0.0.0.0', root).status, FAILED)

    @unittest.skipUnless(common.which('node'), 'Node.js fehlt')
    def test_node_tests_are_counted(self):
        root = repo()
        write(root, 'apps/web/manifest.json', '{}')
        write(root, 'apps/web/a.js', 'module.exports = 1;\n')
        write(root, 'apps/web/tests/a.test.js',
              "const test = require('node:test');\ntest('eins', () => {});\ntest('zwei', () => {});\n")
        results = {r.step: r for r in check.check_web(root)}
        self.assertEqual(results['syntax'].status, OK)
        self.assertEqual(results['tests'].status, OK, results['tests'].detail)
        self.assertEqual(results['tests'].tests, 2)

    @unittest.skipUnless(common.which('node'), 'Node.js fehlt')
    def test_syntax_error_is_found(self):
        root = repo()
        write(root, 'apps/web/manifest.json', '{}')
        write(root, 'apps/web/a.js', 'let = = ;\n')
        results = {r.step: r for r in check.check_web(root)}
        self.assertEqual(results['syntax'].status, FAILED)


@unittest.skipUnless(common.which('go'), 'Go fehlt')
class Desktop(unittest.TestCase):
    def make(self, test_body):
        root = repo()
        write(root, 'apps/desktop/go.mod', 'module osk\n\ngo 1.21\n')
        write(root, 'apps/desktop/main.go',
              'package main\n\nvar version = "dev"\n\nfunc main() { println(version) }\n')
        write(root, 'apps/desktop/main_test.go',
              'package main\n\nimport "testing"\n\n' + test_body)
        return root

    def test_build_stamps_version_and_tests_are_counted(self):
        root = self.make('func TestA(t *testing.T) {}\nfunc TestB(t *testing.T) {}\n')
        result = build.build_desktop('1.2.3.4', root)
        self.assertEqual(result.status, OK, result.detail)
        self.assertTrue([n for n in os.listdir(os.path.join(root, 'dist')) if '1.2.3.4' in n])
        results = {r.step: r for r in check.check_desktop(root)}
        self.assertEqual(results['tests'].tests, 2)
        self.assertEqual(results['tests'].status, OK)

    def test_red_test_fails_the_check(self):
        root = self.make('func TestA(t *testing.T) { t.Fatal("rot") }\n')
        results = {r.step: r for r in check.check_desktop(root)}
        self.assertEqual(results['tests'].status, FAILED)


class Lang(unittest.TestCase):
    def files(self, root, mapping):
        for lang, data in mapping.items():
            write(root, 'apps/web/lang/%s.json' % lang, json.dumps(data))

    def test_same_keys_in_five_languages_pass(self):
        root = repo()
        self.files(root, {l: {'a': 'x', 'b': {'c': 'y'}} for l in check.LANGUAGES})
        self.assertEqual(check.check_lang(root)[0].status, OK)

    def test_missing_key_and_missing_language_fail(self):
        root = repo()
        data = {l: {'a': 'x', 'b': 'y'} for l in check.LANGUAGES}
        data['fr'] = {'a': 'x'}
        del data['it']
        self.files(root, data)
        result = check.check_lang(root)[0]
        self.assertEqual(result.status, FAILED)
        self.assertIn('it.json fehlt', result.detail)
        self.assertIn('fr: fehlt b', result.detail)

    def test_no_lang_folder_is_skipped(self):
        self.assertEqual(check.check_lang(repo())[0].status, SKIPPED)


class Release(unittest.TestCase):
    def test_version_must_be_a_release(self):
        self.assertEqual(release.release_version(repo('1.0.0.0')), '1.0.0.0')
        for bad in ('0.0.0.0', '0.1.1.0', '0.1.0.3'):
            with self.assertRaises(ValueError):
                release.release_version(repo(bad))

    def test_parts_must_agree(self):
        root = repo('0.1.0.0')
        write(root, 'apps/web/VERSION', '0.2.0.0\n')
        with self.assertRaises(ValueError):
            release.release_version(root)

    def test_changelog_moves_unreleased(self):
        text = '# Changelog\n\n## [Unreleased]\n\n### Added\n- a\n\n## [0.1.0.0] - 2026-01-01\n\n- alt\n'
        out = release.release_changelog(text, '0.2.0.0', '2026-10-23')
        self.assertIn('## [Unreleased]\n\n## [0.2.0.0] - 2026-10-23\n\n### Added\n- a\n\n## [0.1.0.0]', out)

    def test_empty_unreleased_is_refused(self):
        with self.assertRaises(ValueError):
            release.release_changelog('# C\n\n## [Unreleased]\n\n', '1.0.0.0', 'x')

    def test_collect_writes_checksums_of_this_version_only(self):
        root = repo()
        write(root, 'dist/osk-web-1.0.0.0.zip', 'neu')
        write(root, 'dist/osk-web-0.9.0.0.zip', 'alt')
        target, names = release.collect(root, '1.0.0.0')
        self.assertEqual(names, ['osk-web-1.0.0.0.zip'])
        sums = io.open(os.path.join(target, 'SHA256SUMS.txt'), encoding='utf-8').read()
        self.assertEqual(len(sums.split()[0]), 64)
        self.assertIn('osk-web-1.0.0.0.zip', sums)
        self.assertNotIn('0.9.0.0', sums)


class Flash(unittest.TestCase):
    def test_without_image_it_says_so_and_fails(self):
        self.assertEqual(flash.main(['dk'], repo()), 1)

    def test_wrong_file_kind_is_refused(self):
        self.assertIn('.hex', flash.flash_dk('x.uf2', '.'))
        self.assertIn('.uf2', flash.flash_dongle('x.hex', None))

    def test_dongle_copies_to_given_drive(self):
        root = repo()
        image = write(root, 'dist/osk-firmware-1-nrf52840dongle.uf2', 'uf2')
        drive = tempfile.mkdtemp()
        self.assertIsNone(flash.flash_dongle(image, drive))
        self.assertTrue(os.path.isfile(os.path.join(drive, os.path.basename(image))))


if __name__ == '__main__':
    unittest.main()
