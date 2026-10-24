# -*- coding: utf-8 -*-
"""Firmware-Paket unterschreiben - nur unterschriebene Pakete nimmt das Geraet an.

    python tools/sign.py --gen-key                         # Schluessel anlegen
    python tools/sign.py --pubkey                          # oeffentlichen Teil zeigen
    python tools/sign.py --pubkey-c                         # als C-Datei fuer den Bootloader
    python tools/sign.py dist/osk-firmware-...hex --board osk_v1 --counter 1
    python tools/sign.py --verify dist/osk-0.0.1.0-osk_v1.oskfw
    python tools/sign.py --selftest                         # RFC-8032-Vektoren

DER SCHLUESSEL LIEGT AUSSERHALB DES PROJEKTORDNERS UND NIE IM REPO.
Vorgabe: %USERPROFILE%\\.osk-keys\\firmware-ed25519.key (Linux:
~/.osk-keys/...), anders ueber --key oder OSK_SIGN_KEY. Dieses Werkzeug
BRICHT AB, wenn der Schluesselpfad im Projektordner liegt - auch wenn er
ausdruecklich genannt wurde. Ein Schluessel, der einmal in einem Commit
steckt, ist aus der Geschichte nicht mehr herauszuholen.

Das Paket: Kopf (72 Byte) · Signatur (64 Byte) · Abbild. Die Signatur deckt
den Kopf, im Kopf steht der SHA-256 des Abbilds - Aufbau und Begruendung in
docs/update.md und apps/firmware/boot/osk_boot.h.

Nur Standardbibliothek. Ist `cryptography` installiert, prueft --selftest
beide Wege gegeneinander.
"""
import argparse
import binascii
import hashlib
import io
import os
import stat
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import common
import ed25519

MAGIC = b'OSKUPD1\x00'
HEADER_LEN = 72
SIG_LEN = 64
HASH_LEN = 32
FORMAT = 1

# Dieselben Zahlen wie osk_board_t in apps/firmware/boot/osk_boot.h.
BOARDS = {
    'nrf52840dk': 1,
    'nrf52840dongle': 2,
    'osk_v1': 3,
}

DEFAULT_KEY = os.path.join(os.path.expanduser('~'), '.osk-keys', 'firmware-ed25519.key')


class SignError(Exception):
    """Etwas stimmt nicht - mit einem Satz, den ein Mensch lesen kann."""


# --- Schluessel -------------------------------------------------------------

def key_path(given=None):
    return os.path.abspath(given or os.environ.get('OSK_SIGN_KEY') or DEFAULT_KEY)


def refuse_inside_project(path, root=common.ROOT):
    """Bricht ab, wenn der Schluessel im Projektordner liegt."""
    path = os.path.abspath(path)
    root = os.path.abspath(root)
    try:
        inside = os.path.commonpath([path, root]) == root
    except ValueError:
        inside = False  # verschiedene Laufwerke
    if inside:
        raise SignError(
            'Der Schluessel darf nicht im Projektordner liegen: %s\n'
            'Er gehoert ausserhalb, etwa nach %s - siehe docs/update.md.' % (path, DEFAULT_KEY))
    return path


def read_key(path):
    path = refuse_inside_project(path)
    if not os.path.isfile(path):
        raise SignError('Kein Schluessel unter %s.\n'
                        'Anlegen mit: python tools/sign.py --gen-key' % path)
    with io.open(path, encoding='utf-8') as handle:
        text = handle.read().strip()
    try:
        seed = binascii.unhexlify(text)
    except (binascii.Error, ValueError):
        raise SignError('%s ist kein Schluessel: erwartet werden 64 Zeichen 0-9a-f.' % path)
    if len(seed) != 32:
        raise SignError('%s hat %d Byte, 32 werden gebraucht.' % (path, len(seed)))
    return seed


def write_key(path, seed):
    path = refuse_inside_project(path)
    folder = os.path.dirname(path)
    if folder and not os.path.isdir(folder):
        os.makedirs(folder)
    if os.path.exists(path):
        raise SignError('%s gibt es schon. Ein vorhandener Schluessel wird nie ueberschrieben -\n'
                        'mit ihm unterschriebene Geraete liessen sich sonst nie mehr erreichen.'
                        % path)
    with io.open(path, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(binascii.hexlify(seed).decode('ascii') + '\n')
    try:
        os.chmod(path, stat.S_IRUSR | stat.S_IWUSR)  # nur der Besitzer, auf POSIX
    except OSError:
        pass
    return path


def gen_key(path):
    seed = os.urandom(32)
    written = write_key(path, seed)
    return written, ed25519.public_key(seed)


# --- Paket ------------------------------------------------------------------

def build_header(board_id, version, counter, image):
    """Kopf ohne Signatur (72 Byte)."""
    numbers = common.parse_version(version)
    head = MAGIC
    head += struct.pack('<HH', HEADER_LEN, FORMAT)
    head += struct.pack('<I', board_id)
    head += struct.pack('<4I', *numbers)
    head += struct.pack('<II', counter, len(image))
    head += hashlib.sha256(image).digest()
    assert len(head) == HEADER_LEN, len(head)
    return head


def pack(seed, board_id, version, counter, image):
    head = build_header(board_id, version, counter, image)
    return head + ed25519.sign(seed, head) + image


def unpack(data):
    """Kopf lesen und pruefen. Gibt ein dict zurueck oder wirft SignError."""
    if len(data) < HEADER_LEN + SIG_LEN:
        raise SignError('Zu kurz fuer ein Paket: %d Byte, mindestens %d.'
                        % (len(data), HEADER_LEN + SIG_LEN))
    if data[:8] != MAGIC:
        raise SignError('Kein Paket dieses Projekts (Kennung fehlt).')
    header_len, fmt = struct.unpack('<HH', data[8:12])
    if header_len != HEADER_LEN or fmt != FORMAT:
        raise SignError('Unbekannter Aufbau: Kopflaenge %d, Format %d.' % (header_len, fmt))
    board_id = struct.unpack('<I', data[12:16])[0]
    version = common.format_version(struct.unpack('<4I', data[16:32]))
    counter, image_len = struct.unpack('<II', data[32:40])
    image_hash = data[40:72]
    image = data[HEADER_LEN + SIG_LEN:]
    if len(image) != image_len:
        raise SignError('Das Abbild ist %d Byte lang, der Kopf sagt %d.'
                        % (len(image), image_len))
    return {
        'board_id': board_id,
        'board': next((n for n, i in BOARDS.items() if i == board_id), 'unbekannt'),
        'version': version,
        'counter': counter,
        'image_len': image_len,
        'image_hash': image_hash,
        'header': data[:HEADER_LEN],
        'signature': data[HEADER_LEN:HEADER_LEN + SIG_LEN],
        'image': image,
    }


def verify(data, pub):
    """True, wenn Signatur UND Abbild-Hash stimmen."""
    info = unpack(data)
    if not ed25519.verify(pub, info['header'], info['signature']):
        return False, 'Die Signatur passt nicht zu diesem oeffentlichen Schluessel.'
    if hashlib.sha256(info['image']).digest() != info['image_hash']:
        return False, 'Der Hash des Abbilds stimmt nicht - das Paket ist beschaedigt.'
    return True, 'Signatur und Abbild in Ordnung (%s, Fassung %s, Zaehler %d, %d Byte).' % (
        info['board'], info['version'], info['counter'], info['image_len'])


# --- C-Datei fuer den Bootloader -------------------------------------------

def pubkey_c(pub):
    lines = ['/* Oeffentlicher Schluessel fuer die Pruefung der Firmware-Pakete.',
             ' * ERZEUGT von tools/sign.py --pubkey-c - nicht von Hand aendern.',
             ' * Der private Teil liegt ausserhalb des Projektordners. */',
             '#ifndef OSK_BOOT_PUBKEY_H',
             '#define OSK_BOOT_PUBKEY_H',
             '',
             '#include <stdint.h>',
             '',
             'static const uint8_t osk_boot_pubkey[32] = {']
    for start in range(0, 32, 8):
        row = ', '.join('0x%02x' % b for b in pub[start:start + 8])
        lines.append('\t' + row + ',')
    lines += ['};', '', '#endif']
    return '\n'.join(lines) + '\n'


# --- Selbstpruefung ---------------------------------------------------------

# RFC 8032, Abschnitt 7.1 - TEST 1, TEST 2 und TEST 3.
RFC_VECTORS = [
    ('9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60',
     'd75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a',
     '',
     'e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc'
     '61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b'),
    ('4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb',
     '3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c',
     '72',
     '92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e4'
     '58f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00'),
    ('c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7',
     'fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025',
     'af82',
     '6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290a'
     'e67f760984dc6594a7c15e9716ed28dc027beceea1ec40a'),
]


def selftest():
    """Gibt (Zahl der Fehler, Zeilen) zurueck."""
    wrong, lines = 0, []
    for seed_hex, pub_hex, msg_hex, sig_hex in RFC_VECTORS:
        seed = binascii.unhexlify(seed_hex)
        pub = binascii.unhexlify(pub_hex)
        msg = binascii.unhexlify(msg_hex)
        sig = binascii.unhexlify(sig_hex)
        got_pub = ed25519.public_key(seed)
        got_sig = ed25519.sign(seed, msg)
        good = got_pub == pub and got_sig == sig and ed25519.verify(pub, msg, sig)
        # Und die Gegenprobe: eine veraenderte Nachricht darf nicht passen.
        good = good and not ed25519.verify(pub, msg + b'\x00', sig)
        wrong += 0 if good else 1
        lines.append('RFC 8032 (%d Byte Nachricht): %s' % (len(msg), 'richtig' if good else 'FALSCH'))

    try:
        from cryptography.hazmat.primitives.asymmetric import ed25519 as other
    except ImportError:
        lines.append("zweiter Weg: uebersprungen - Paket 'cryptography' ist nicht da")
    else:
        seed = os.urandom(32)
        key = other.Ed25519PrivateKey.from_private_bytes(seed)
        same = (key.sign(b'opensecurekey') == ed25519.sign(seed, b'opensecurekey'))
        wrong += 0 if same else 1
        lines.append('zweiter Weg (cryptography): %s' % ('gleiches Ergebnis' if same else 'ANDERS'))
    return wrong, lines


# --- Aufruf -----------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('image', nargs='?', help='Abbild, das unterschrieben wird')
    parser.add_argument('--board', choices=sorted(BOARDS), help='fuer welches Brett')
    parser.add_argument('--counter', type=int, help='Rueckschritt-Zaehler, steigt nie ab')
    parser.add_argument('--version', help='Vorgabe: apps/firmware/VERSION')
    parser.add_argument('--out', help='Zieldatei, Vorgabe dist/osk-<Fassung>-<Brett>.oskfw')
    parser.add_argument('--key', help='Schluesselpfad; nie im Projektordner')
    parser.add_argument('--gen-key', action='store_true')
    parser.add_argument('--pubkey', action='store_true')
    parser.add_argument('--pubkey-c', action='store_true')
    parser.add_argument('--verify', metavar='PAKET')
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args(argv)

    try:
        if args.selftest:
            wrong, lines = selftest()
            print('\n'.join(lines))
            return 1 if wrong else 0

        if args.gen_key:
            path, pub = gen_key(key_path(args.key))
            print('Schluessel angelegt: %s' % path)
            print('Oeffentlicher Teil:  %s' % binascii.hexlify(pub).decode('ascii'))
            print('\nDer private Teil darf nie ins Repo und nie in eine Sicherung, die')
            print('weitergegeben wird. Geht er verloren, nimmt kein Geraet mehr ein Update an.')
            return 0

        if args.pubkey or args.pubkey_c:
            pub = ed25519.public_key(read_key(key_path(args.key)))
            if args.pubkey_c:
                sys.stdout.write(pubkey_c(pub))
            else:
                print(binascii.hexlify(pub).decode('ascii'))
            return 0

        if args.verify:
            with io.open(args.verify, 'rb') as handle:
                data = handle.read()
            pub = ed25519.public_key(read_key(key_path(args.key)))
            good, text = verify(data, pub)
            print(text)
            return 0 if good else 1

        if not args.image:
            parser.error('ein Abbild angeben, oder --gen-key / --pubkey / --verify / --selftest')
        if not args.board:
            parser.error('--board angeben: ein Paket ohne Brettkennung nimmt kein Geraet an')
        if args.counter is None:
            parser.error('--counter angeben (Rueckschritt-Zaehler, siehe docs/update.md)')
        if args.counter < 0 or args.counter > 0xFFFFFFFF:
            parser.error('--counter muss zwischen 0 und 4294967295 liegen')

        seed = read_key(key_path(args.key))
        version = args.version or common.read_version('firmware')
        with io.open(args.image, 'rb') as handle:
            image = handle.read()
        if not image:
            raise SignError('%s ist leer.' % args.image)
        data = pack(seed, BOARDS[args.board], version, args.counter, image)

        out = args.out or os.path.join(common.ROOT, 'dist',
                                       'osk-%s-%s.oskfw' % (version, args.board))
        folder = os.path.dirname(os.path.abspath(out))
        if folder and not os.path.isdir(folder):
            os.makedirs(folder)
        with io.open(out, 'wb') as handle:
            handle.write(data)
        print('%s  (%d Byte Abbild, Fassung %s, %s, Zaehler %d)'
              % (out, len(image), version, args.board, args.counter))
        print('SHA-256 des Abbilds: %s' % hashlib.sha256(image).hexdigest())
        return 0
    except (SignError, ValueError) as error:
        sys.stderr.write('%s\n' % error)
        return 1
    except OSError as error:
        sys.stderr.write('%s\n' % error)
        return 1


if __name__ == '__main__':
    sys.exit(main())
