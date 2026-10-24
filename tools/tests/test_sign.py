# -*- coding: utf-8 -*-
"""Tests fuer sign.py, dfu.py und ed25519.py - Pakete, Signaturen, Ablauf.

Kein Geraet beteiligt. Die Gegenstelle in FakeDevice bildet nach, was
apps/firmware/boot/boot_session.c tut; geprueft wird damit die Seite des
Werkzeugs, nicht das Geraet.
"""
import binascii
import hashlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import common
import dfu
import ed25519
import sign


def outside_key():
    """Ein Schluesselpfad ausserhalb des Projektordners - sign.py verweigert
    jeden anderen."""
    folder = tempfile.mkdtemp(prefix='osk-key-')
    return os.path.join(folder, 'firmware-ed25519.key')


class TestEd25519(unittest.TestCase):
    def test_rfc_8032_vektoren(self):
        """Die offiziellen Vektoren aus RFC 8032, Abschnitt 7.1."""
        wrong, lines = sign.selftest()
        self.assertEqual(0, wrong, '\n'.join(lines))
        self.assertTrue(any('RFC 8032' in line for line in lines), lines)

    def test_veraenderte_nachricht_passt_nicht(self):
        seed = b'\x01' * 32
        pub = ed25519.public_key(seed)
        sig = ed25519.sign(seed, b'hallo')
        self.assertTrue(ed25519.verify(pub, b'hallo', sig))
        self.assertFalse(ed25519.verify(pub, b'hallp', sig))
        self.assertFalse(ed25519.verify(pub, b'hallo', bytes(64)))

    def test_fremder_schluessel_passt_nicht(self):
        sig = ed25519.sign(b'\x02' * 32, b'x')
        self.assertFalse(ed25519.verify(ed25519.public_key(b'\x03' * 32), b'x', sig))

    def test_zu_grosses_s_wird_abgelehnt(self):
        """Eine zweite gueltige Form derselben Signatur - RFC 8032 verlangt
        die Ablehnung, sonst gibt es zwei Pakete mit gleichem Inhalt und
        verschiedener Signatur."""
        seed = b'\x04' * 32
        pub = ed25519.public_key(seed)
        sig = ed25519.sign(seed, b'x')
        s = int.from_bytes(sig[32:], 'little') + ed25519.L
        self.assertFalse(ed25519.verify(pub, b'x', sig[:32] + s.to_bytes(32, 'little')))

    def test_falsche_laengen(self):
        self.assertFalse(ed25519.verify(b'kurz', b'x', bytes(64)))
        self.assertFalse(ed25519.verify(bytes(32), b'x', b'kurz'))
        with self.assertRaises(ValueError):
            ed25519.sign(b'kurz', b'x')
        with self.assertRaises(ValueError):
            ed25519.public_key(b'kurz')


class TestSchluessel(unittest.TestCase):
    def test_schluessel_im_projektordner_wird_verweigert(self):
        """Ein Schluessel im Repo ist aus der Geschichte nicht mehr
        herauszuholen. Darum nicht einmal auf ausdruecklichen Wunsch."""
        for rel in ('geheim.key', 'tools/geheim.key', 'apps/firmware/boot/x.key'):
            with self.assertRaises(sign.SignError):
                sign.refuse_inside_project(os.path.join(common.ROOT, rel))

    def test_schluessel_ausserhalb_ist_in_ordnung(self):
        path = outside_key()
        self.assertEqual(os.path.abspath(path), sign.refuse_inside_project(path))

    def test_anlegen_lesen_oeffentlicher_teil(self):
        path = outside_key()
        written, pub = sign.gen_key(path)
        self.assertEqual(os.path.abspath(path), written)
        self.assertEqual(32, len(pub))
        self.assertEqual(pub, ed25519.public_key(sign.read_key(path)))

    def test_vorhandener_schluessel_wird_nie_ueberschrieben(self):
        path = outside_key()
        sign.gen_key(path)
        with self.assertRaises(sign.SignError):
            sign.gen_key(path)

    def test_unbrauchbare_datei_sagt_warum(self):
        path = outside_key()
        with io.open(path, 'w', encoding='utf-8') as handle:
            handle.write('kein schluessel\n')
        with self.assertRaises(sign.SignError):
            sign.read_key(path)
        with io.open(path, 'w', encoding='utf-8') as handle:
            handle.write('aabb\n')  # richtige Zeichen, falsche Laenge
        with self.assertRaises(sign.SignError):
            sign.read_key(path)

    def test_fehlender_schluessel_nennt_den_befehl(self):
        with self.assertRaises(sign.SignError) as box:
            sign.read_key(outside_key())
        self.assertIn('--gen-key', str(box.exception))

    def test_oeffentlicher_teil_als_c_datei(self):
        pub = ed25519.public_key(b'\x05' * 32)
        text = sign.pubkey_c(pub)
        self.assertIn('osk_boot_pubkey[32]', text)
        self.assertEqual(4, text.count('\n\t'))  # 4 Zeilen mit je 8 Byte
        for byte in pub:
            self.assertIn('0x%02x' % byte, text)


class TestPaket(unittest.TestCase):
    def setUp(self):
        self.seed = b'\x06' * 32
        self.pub = ed25519.public_key(self.seed)
        self.image = bytes((i * 7) % 256 for i in range(3000))
        self.pkg = sign.pack(self.seed, sign.BOARDS['osk_v1'], '0.0.2.0', 5, self.image)

    def test_aufbau_stimmt_mit_dem_bootloader_ueberein(self):
        """Dieselben Zahlen wie apps/firmware/boot/osk_boot.h."""
        self.assertEqual(72, sign.HEADER_LEN)
        self.assertEqual(64, sign.SIG_LEN)
        self.assertEqual(b'OSKUPD1\x00', sign.MAGIC)
        self.assertEqual(72 + 64 + len(self.image), len(self.pkg))
        self.assertEqual({'nrf52840dk': 1, 'nrf52840dongle': 2, 'osk_v1': 3}, sign.BOARDS)

    def test_lesen_was_drin_steht(self):
        info = sign.unpack(self.pkg)
        self.assertEqual('osk_v1', info['board'])
        self.assertEqual('0.0.2.0', info['version'])
        self.assertEqual(5, info['counter'])
        self.assertEqual(len(self.image), info['image_len'])
        self.assertEqual(hashlib.sha256(self.image).digest(), info['image_hash'])
        self.assertEqual(self.image, info['image'])

    def test_gutes_paket_wird_bestaetigt(self):
        good, text = sign.verify(self.pkg, self.pub)
        self.assertTrue(good, text)
        self.assertIn('osk_v1', text)

    def test_jedes_verdrehte_byte_des_kopfes_faellt_auf(self):
        for pos in range(sign.HEADER_LEN):
            broken = bytearray(self.pkg)
            broken[pos] ^= 0x01
            good, _ = (False, '')
            try:
                good, _ = sign.verify(bytes(broken), self.pub)
            except sign.SignError:
                good = False  # Kennung oder Format - auch eine Ablehnung
            self.assertFalse(good, 'Byte %d des Kopfes blieb unbemerkt' % pos)

    def test_verdrehte_signatur_faellt_auf(self):
        for pos in range(sign.HEADER_LEN, sign.HEADER_LEN + sign.SIG_LEN):
            broken = bytearray(self.pkg)
            broken[pos] ^= 0x80
            good, _ = sign.verify(bytes(broken), self.pub)
            self.assertFalse(good, 'Byte %d der Signatur blieb unbemerkt' % pos)

    def test_verdrehtes_abbild_faellt_am_hash_auf(self):
        broken = bytearray(self.pkg)
        broken[sign.HEADER_LEN + sign.SIG_LEN + 100] ^= 0xFF
        good, text = sign.verify(bytes(broken), self.pub)
        self.assertFalse(good)
        self.assertIn('Hash', text)

    def test_fremder_schluessel_passt_nicht(self):
        other = ed25519.public_key(b'\x07' * 32)
        good, text = sign.verify(self.pkg, other)
        self.assertFalse(good)
        self.assertIn('Signatur', text)

    def test_zu_kurz_und_fremde_kennung(self):
        with self.assertRaises(sign.SignError):
            sign.unpack(self.pkg[:100])
        with self.assertRaises(sign.SignError):
            sign.unpack(b'NICHTOSK' + self.pkg[8:])
        with self.assertRaises(sign.SignError):
            sign.unpack(b'')
        # Richtige Kennung, aber nach 9 Byte zu Ende. Ohne die
        # Laengenpruefung ganz vorn stuerzte das Auspacken hier ab, statt
        # einen Satz zu melden - gefunden von einem Mutanten.
        with self.assertRaises(sign.SignError):
            sign.unpack(sign.MAGIC + b'H')

    def test_abgeschnittenes_abbild_faellt_an_der_laenge(self):
        with self.assertRaises(sign.SignError) as box:
            sign.unpack(self.pkg[:-10])
        self.assertIn('Abbild', str(box.exception))

    def test_unbekanntes_format(self):
        broken = bytearray(self.pkg)
        broken[10] = 9
        with self.assertRaises(sign.SignError):
            sign.unpack(bytes(broken))

    def test_zaehler_und_fassung_landen_im_kopf(self):
        """Die Rueckschritt-Sperre im Geraet liest genau diese zwei Felder."""
        pkg = sign.pack(self.seed, 1, '1.2.3.4', 0xFFFFFFFF, b'x')
        info = sign.unpack(pkg)
        self.assertEqual(0xFFFFFFFF, info['counter'])
        self.assertEqual('1.2.3.4', info['version'])
        self.assertEqual('nrf52840dk', info['board'])


class TestCbor(unittest.TestCase):
    """Nur die vier Sorten, die im Protokoll vorkommen."""

    def test_hin_und_zurueck(self):
        for value in (0, 1, 23, 24, 255, 256, 65535, 65536, 2 ** 32 - 1, 2 ** 32,
                      b'', b'\x00' * 300, 'text', {'op': 1, 'b': b'xy'},
                      {1: 2, 'a': {'b': 3}}):
            self.assertEqual(value, dfu.cbor_load(dfu.cbor_dump(value)), repr(value))

    def test_bekannte_bytefolgen(self):
        """Gegen RFC 8949, Anhang A - sonst prueft der Test nur sich selbst."""
        self.assertEqual(b'\x00', dfu.cbor_dump(0))
        self.assertEqual(b'\x17', dfu.cbor_dump(23))
        self.assertEqual(b'\x18\x18', dfu.cbor_dump(24))
        self.assertEqual(b'\x19\x03\xe8', dfu.cbor_dump(1000))
        self.assertEqual(b'\x44\x01\x02\x03\x04', dfu.cbor_dump(b'\x01\x02\x03\x04'))
        self.assertEqual(b'\x61a', dfu.cbor_dump('a'))
        self.assertEqual(b'\xa1\x61a\x01', dfu.cbor_dump({'a': 1}))

    def test_abbildung_ist_immer_dieselbe_bytefolge(self):
        a = dfu.cbor_dump({'op': 1, 'off': 2, 'b': b'z'})
        b = dfu.cbor_dump({'b': b'z', 'off': 2, 'op': 1})
        self.assertEqual(a, b)

    def test_was_nicht_vorkommt_wird_abgelehnt(self):
        for value in (-1, True, 1.5, None, [1, 2]):
            with self.assertRaises(dfu.DfuError, msg=repr(value)):
                dfu.cbor_dump(value)

    def test_abgeschnittene_antwort(self):
        with self.assertRaises(dfu.DfuError):
            dfu.cbor_load(b'\x19\x03')
        with self.assertRaises(dfu.DfuError):
            dfu.cbor_load(b'')


class FakeDevice:
    """Bildet nach, was boot_session.c tut - so weit das Werkzeug es sieht.

    Nicht das Geraet, und auch kein Beweis ueber das Geraet: hier wird der
    Ablauf der Werkzeugseite gemessen. `button` sagt, ob die Taste gedrueckt
    wird; `fail_at` bricht ab dem n-ten Aufruf ab (gezogener Stecker)."""

    def __init__(self, running='0.0.1.0', counter=0, button=True, fail_at=None):
        self.running = running
        self.counter = counter
        self.button = button
        self.fail_at = fail_at
        self.calls = 0
        self.stage = 0
        self.written = 0
        self.image = bytearray()
        self.head = None
        self.log = []

    def send(self, frame):
        self.calls += 1
        if self.fail_at is not None and self.calls >= self.fail_at:
            raise OSError('Stecker gezogen')
        msg = dfu.cbor_load(frame)
        data = msg.get('d') or {}
        op = data.get('op')
        self.log.append(op)
        result, answer = self._handle(op, data)
        return dfu.cbor_dump({'v': 1, 'c': 13, 's': msg['s'], 'r': result, 'd': answer})

    def _handle(self, op, data):
        if op == dfu.OP_BEGIN:
            head = data.get('hdr') or b''
            if len(head) != sign.HEADER_LEN + sign.SIG_LEN:
                return 9, {}
            if not self.button:
                return 4, {}  # OSK_ERR_DENIED - die Taste entscheidet
            self.head = head
            self.stage, self.written = 1, 0
            self.image = bytearray()
            return 0, {}
        if op == dfu.OP_CHUNK:
            if self.stage != 1:
                return 6, {}
            if data.get('off') != self.written:
                return 9, {}
            part = data.get('b') or b''
            self.image += part
            self.written += len(part)
            return 0, {}
        if op == dfu.OP_FINISH:
            if self.stage != 1:
                return 6, {}
            want = self.head[40:72]
            if hashlib.sha256(bytes(self.image)).digest() != want:
                return 9, {}
            self.stage = 2
            return 0, {}
        if op == dfu.OP_ABORT:
            self.stage, self.written = 0, 0
            return 0, {}
        if op == dfu.OP_STATE:
            return 0, {'stage': self.stage, 'written': self.written,
                       'len': len(self.image), 'ver': self.running}
        return 10, {}


class TestAufspielen(unittest.TestCase):
    def setUp(self):
        self.seed = b'\x08' * 32
        self.image = bytes((i * 13) % 256 for i in range(2500))
        self.pkg = sign.pack(self.seed, sign.BOARDS['osk_v1'], '0.0.2.0', 1, self.image)

    def test_ganzer_ablauf(self):
        device = FakeDevice()
        sent = dfu.upload(dfu.Device(device.send), self.pkg, chunk=512)
        self.assertEqual(len(self.image), sent)
        self.assertEqual(self.image, bytes(device.image))
        self.assertEqual(2, device.stage)  # bereit zum Umschalten
        self.assertEqual([dfu.OP_BEGIN] + [dfu.OP_CHUNK] * 5 + [dfu.OP_FINISH], device.log)

    def test_fortschritt_wird_gemeldet(self):
        seen = []
        dfu.upload(dfu.Device(FakeDevice().send), self.pkg, chunk=1000,
                   progress=lambda done, total: seen.append((done, total)))
        self.assertEqual([(1000, 2500), (2000, 2500), (2500, 2500)], seen)

    def test_ohne_tastendruck_wird_nichts_uebertragen(self):
        """Der Stick fragt vorher. Das Werkzeug kann das nicht umgehen."""
        device = FakeDevice(button=False)
        with self.assertRaises(dfu.DfuError) as box:
            dfu.upload(dfu.Device(device.send), self.pkg)
        self.assertIn('Taste', str(box.exception))
        self.assertEqual(0, device.written)
        self.assertEqual(0, device.stage)
        self.assertEqual([dfu.OP_BEGIN], device.log)

    def test_beschaedigtes_paket_wird_vor_der_uebertragung_abgelehnt(self):
        device = FakeDevice()
        with self.assertRaises(sign.SignError):
            dfu.upload(dfu.Device(device.send), self.pkg[:-5])
        self.assertEqual(0, device.calls)  # kein einziges Byte unterwegs

    def test_verdrehtes_abbild_faellt_beim_abschliessen(self):
        broken = bytearray(self.pkg)
        broken[sign.HEADER_LEN + sign.SIG_LEN + 7] ^= 0xFF
        device = FakeDevice()
        with self.assertRaises(dfu.DfuError) as box:
            dfu.upload(dfu.Device(device.send), bytes(broken))
        self.assertIn('Hash', str(box.exception))
        self.assertEqual(1, device.stage)  # nicht bereit - wird nie gestartet

    def test_abbruch_an_jeder_stelle_laesst_das_geraet_unfertig(self):
        """Gezogener Stecker bei jedem einzelnen Aufruf. Danach steht das
        Geraet nie auf 'bereit' - es startet also die alte Fassung."""
        # 7 Aufrufe: begin, 5 Stuecke, finish. Jeder einzelne wird
        # abgebrochen - und die Zahl wird nicht geraten, sondern am Lauf
        # ohne Abbruch abgelesen (Fall darunter).
        for stop in range(1, 8):
            device = FakeDevice(fail_at=stop)
            with self.assertRaises(OSError):
                dfu.upload(dfu.Device(device.send), self.pkg, chunk=512)
            self.assertNotEqual(2, device.stage, 'Abbruch bei Aufruf %d' % stop)

    def test_zwischenpruefung_derselbe_weg_laeuft_ohne_abbruch_durch(self):
        """Ohne diesen Fall koennte das 'nie bereit' oben auch daran liegen,
        dass der Ablauf gar nicht funktioniert."""
        device = FakeDevice(fail_at=None)
        dfu.upload(dfu.Device(device.send), self.pkg, chunk=512)
        self.assertEqual(2, device.stage)

    def test_abbrechen_setzt_zurueck(self):
        device = FakeDevice()
        handle = dfu.Device(device.send)
        handle.call({'op': dfu.OP_BEGIN, 'hdr': self.pkg[:136]})
        handle.call({'op': dfu.OP_CHUNK, 'off': 0, 'b': self.image[:100]})
        self.assertEqual(100, device.written)
        dfu.abort(handle)
        self.assertEqual(0, device.stage)
        self.assertEqual(0, device.written)

    def test_zustand_lesen(self):
        device = FakeDevice(running='0.0.1.0')
        state = dfu.read_state(dfu.Device(device.send))
        self.assertEqual(0, state['stage'])
        self.assertEqual('nichts im Gange', state['stage_text'])
        self.assertEqual('0.0.1.0', state['version'])

    def test_antwort_mit_falscher_folgenummer_wird_abgelehnt(self):
        def crooked(frame):
            msg = dfu.cbor_load(frame)
            return dfu.cbor_dump({'v': 1, 'c': 13, 's': msg['s'] + 1, 'r': 0, 'd': {}})

        with self.assertRaises(dfu.DfuError) as box:
            dfu.Device(crooked).call({'op': dfu.OP_STATE})
        self.assertIn('andere', str(box.exception))

    def test_antwort_in_falscher_form(self):
        with self.assertRaises(dfu.DfuError):
            dfu.Device(lambda frame: dfu.cbor_dump(7)).call({'op': dfu.OP_STATE})

    def test_jeder_fehlergrund_hat_einen_satz(self):
        """Dieselben Zahlen wie osk_result_t - ohne Luecke, sonst steht am
        Geraet eine Zahl statt eines Satzes."""
        for code in range(12):
            self.assertIn(code, dfu.RESULTS)
            self.assertNotIn('unbekannt', dfu.result_text(code))
        self.assertIn('unbekannt', dfu.result_text(99))

    def test_unbekannter_unterbefehl(self):
        device = FakeDevice()
        with self.assertRaises(dfu.DfuError) as box:
            dfu.Device(device.send).call({'op': 99})
        self.assertIn('Brett', str(box.exception))  # UNSUPPORTED

    def test_schritte_von_hand_nennen_die_datei_und_den_rueckweg(self):
        info = sign.unpack(self.pkg)
        text = dfu.manual_steps('dist/paket.oskfw', info)
        self.assertIn('Taste gedrueckt halten und einstecken', text)
        self.assertIn('alte Fassung', text)
        self.assertIn('Tresor', text)
        self.assertIn('osk_v1', text)


if __name__ == '__main__':
    unittest.main()
