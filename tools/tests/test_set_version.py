# -*- coding: utf-8 -*-
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import common
import set_version


def make_repo(versions):
    root = tempfile.mkdtemp()
    for part, text in versions.items():
        os.makedirs(os.path.join(root, 'apps', part))
        with io.open(common.version_file(part, root), 'w', encoding='utf-8') as handle:
            handle.write(text + '\n')
    return root


def zeros():
    return make_repo({'firmware': '0.0.0.0', 'desktop': '0.0.0.0', 'web': '0.0.0.0'})


class Bump(unittest.TestCase):
    def test_example_table_of_version_md(self):
        steps = [('build', '0.0.0.1'), ('build', '0.0.0.2'), ('feature', '0.0.1.0'),
                 ('build', '0.0.1.1'), ('release', '0.1.0.0'), ('build', '0.1.0.1'),
                 ('feature', '0.1.1.0'), ('release', '0.2.0.0'), ('version', '1.0.0.0')]
        version = '0.0.0.0'
        for kind, expected in steps:
            version = common.bump(version, kind)
            self.assertEqual(version, expected)

    def test_right_places_reset(self):
        self.assertEqual(common.bump('1.2.3.4', 'feature'), '1.2.4.0')
        self.assertEqual(common.bump('1.2.3.4', 'release'), '1.3.0.0')
        self.assertEqual(common.bump('1.2.3.4', 'version'), '2.0.0.0')

    def test_rejects_wrong_format(self):
        for bad in ('1.0.0', '1.0.0.0.0', 'v1.0.0.0', '1.0.0.0-beta', '', '1.a.0.0'):
            with self.assertRaises(ValueError):
                common.parse_version(bad)


ZEPHYR = ('VERSION_MAJOR = 1\nVERSION_MINOR = 2\n'
          'PATCHLEVEL = 3\nVERSION_TWEAK = 4\n')


class ZephyrSchreibweise(unittest.TestCase):
    """Die Firmware fuehrt ihre Nummer in Zephyrs Schreibweise, weil Zephyr
    apps/firmware/VERSION selbst liest. Es ist dieselbe vierstellige Nummer."""

    def test_reads_zephyr_form(self):
        self.assertEqual(common.parse_version_file(ZEPHYR), (1, 2, 3, 4))

    def test_reads_house_form(self):
        self.assertEqual(common.parse_version_file('1.2.3.4\n'), (1, 2, 3, 4))

    def test_incomplete_zephyr_form_is_an_error(self):
        # Gegenprobe: eine fehlende Zeile darf nicht stillschweigend 0 ergeben.
        with self.assertRaises(ValueError):
            common.parse_version_file('VERSION_MAJOR = 1\nVERSION_MINOR = 2\n')

    def test_bump_keeps_the_zephyr_form(self):
        root = make_repo({'firmware': ZEPHYR.strip(), 'desktop': '0.0.0.0',
                          'web': '0.0.0.0'})
        self.assertEqual(set_version.main(['firmware', '--build'], root), 0)
        with io.open(common.version_file('firmware', root), encoding='utf-8') as handle:
            text = handle.read()
        self.assertIn('VERSION_TWEAK = 5', text)
        self.assertEqual(common.read_version('firmware', root), '1.2.3.5')

    def test_bump_keeps_the_house_form(self):
        root = zeros()
        self.assertEqual(set_version.main(['desktop', '--build'], root), 0)
        with io.open(common.version_file('desktop', root), encoding='utf-8') as handle:
            self.assertEqual(handle.read(), '0.0.0.1\n')

    def test_the_real_firmware_file_is_zephyr_form(self):
        # Sonst bricht der ARM-Bau, und zwar erst spaet.
        self.assertTrue(common.is_zephyr_version('firmware'))
        self.assertEqual(common.read_version('firmware'), '0.0.1.0')


class Cli(unittest.TestCase):
    def test_build_writes_only_the_version_file(self):
        root = zeros()
        self.assertEqual(set_version.main(['desktop', '--build'], root), 0)
        self.assertEqual(common.read_version('desktop', root), '0.0.0.1')
        self.assertEqual(common.read_version('firmware', root), '0.0.0.0')
        self.assertEqual(common.read_version('web', root), '0.0.0.0')
        self.assertEqual(os.listdir(os.path.join(root, 'apps', 'desktop')), ['VERSION'])

    def test_all_counts_each_part(self):
        root = make_repo({'firmware': '0.1.0.5', 'desktop': '0.0.2.0', 'web': '0.0.0.0'})
        self.assertEqual(set_version.main(['all', '--release'], root), 0)
        self.assertEqual([common.read_version(p, root) for p in common.PARTS],
                         ['0.2.0.0', '0.1.0.0', '0.1.0.0'])

    def test_broken_file_changes_nothing(self):
        root = make_repo({'firmware': '0.0.0.0', 'desktop': 'kaputt', 'web': '0.0.0.0'})
        self.assertEqual(set_version.main(['all', '--build'], root), 1)
        self.assertEqual(common.read_version('firmware', root), '0.0.0.0')

    def test_needs_part_and_kind(self):
        root = zeros()
        with self.assertRaises(SystemExit):
            set_version.main(['web'], root)
        with self.assertRaises(SystemExit):
            set_version.main(['--build'], root)

    def test_check_reports_error(self):
        self.assertEqual(set_version.check(zeros())[0], 0)
        bad = make_repo({'firmware': '0.0.0', 'desktop': '0.0.0.0', 'web': '0.0.0.0'})
        self.assertEqual(set_version.check(bad)[0], 1)


class Windows(unittest.TestCase):
    def test_command_line_quotes_for_cmd(self):
        self.assertEqual(common.command_line(['west', 'build', '-d', 'a b']), 'west build -d "a b"')

    def test_missing_program_is_code_127_not_traceback(self):
        code, _ = common.run(['gibt-es-nicht-xyz'], windows=False)
        self.assertEqual(code, 127)


if __name__ == '__main__':
    unittest.main()
