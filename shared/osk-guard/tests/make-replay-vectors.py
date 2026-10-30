# -*- coding: utf-8 -*-
"""Erzeugt replay-vectors.json: echte Antworten des Geraets, zum Wiedereinreichen.

    python shared/osk-guard/tests/make-replay-vectors.py

Die Datei wird eingecheckt, damit alle fuenf Sprachen dieselben Bytes
pruefen und keine davon einen Unterschreiber braucht. Unterschrieben wird
hier mit tools/ed25519.py - eigenem Code dieses Projekts.

Jeder Fall ist eine ECHTE Antwort (gueltig fuer die Frage, zu der sie
gehoert) und eine zweite Frage, zu der sie NICHT gehoert. Eine Bibliothek,
die den zweiten Fall annimmt, nimmt eine Aufzeichnung an.
"""
import hashlib
import io
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import ed25519                                   # noqa: E402  eigener Code

ATTEST = b'osk-guard/attest/1'
ATTEST_RAW = b'osk-guard/attest-raw/1'
SIGN = b'osk-guard/sign/1'


def message(domain, nonce, bin_hash):
    return domain + b'\x00' + nonce + bin_hash


def hexs(raw):
    return raw.hex()


def main():
    seed = bytes(range(32))                      # das echte Geraet
    other = bytes((17 + i) % 251 for i in range(32))   # ein zweites Geraet
    pub = ed25519.public_key(seed)
    other_pub = ed25519.public_key(other)

    nonce_a = hashlib.sha256(b'frage a').digest()
    nonce_b = hashlib.sha256(b'frage b').digest()
    program = hashlib.sha256(b'das geschuetzte programm').digest()
    patched = hashlib.sha256(b'das gepatchte programm').digest()

    sig_a = ed25519.sign(seed, message(ATTEST, nonce_a, program))
    sig_patched = ed25519.sign(seed, message(ATTEST, nonce_a, patched))
    sig_other = ed25519.sign(other, message(ATTEST, nonce_a, program))
    # Die alte Form: freie Laenge, EIGENE Zweckzeichenfolge. 64 Byte
    # Aufgabe sind genau so lang wie Zufallswert plus Programmhash - ohne
    # getrennte Zwecke waere diese Unterschrift fuer den Dongle gueltig.
    sig_raw = ed25519.sign(seed, ATTEST_RAW + b'\x00' + nonce_a + program)
    # Eine Auftragsunterschrift, als Praesenzantwort eingereicht.
    sig_job = ed25519.sign(seed, SIGN + b'\x00' + b'prod-eu-1' + b'\x00'
                           + hashlib.sha256(b'ein auftrag').digest())

    cases = [
        {'name': 'echte Antwort auf ihre eigene Frage',
         'nonce': hexs(nonce_a), 'binhash': hexs(program),
         'pub': hexs(pub), 'sig': hexs(sig_a), 'valid': True,
         'expect': '',
         'why': 'die Messlatte: ohne diesen Fall beweist kein abgewiesener '
                'Fall etwas'},
        {'name': 'dieselbe Antwort auf eine neue Frage (Aufzeichnung)',
         'nonce': hexs(nonce_b), 'binhash': hexs(program),
         'pub': hexs(pub), 'sig': hexs(sig_a), 'valid': False,
         'expect': 'error.guard.signature',
         'why': 'eine aufgezeichnete Antwort ist die Antwort auf eine andere '
                'Frage'},
        {'name': 'Antwort fuer eine gepatchte Programmdatei',
         'nonce': hexs(nonce_a), 'binhash': hexs(program),
         'pub': hexs(pub), 'sig': hexs(sig_patched), 'valid': False,
         'expect': 'error.guard.signature',
         'why': 'die Programmdatei steckt in der Frage'},
        {'name': 'echte Unterschrift, aber ein anderes Geraet',
         'nonce': hexs(nonce_a), 'binhash': hexs(program),
         'pub': hexs(pub), 'sig': hexs(sig_other), 'valid': False,
         'expect': 'error.guard.signature',
         'why': 'geprueft wird gegen den eingetragenen Schluessel'},
        {'name': 'die alte attest-Form mit Zufallswert und Hash als Aufgabe',
         'nonce': hexs(nonce_a), 'binhash': hexs(program),
         'pub': hexs(pub), 'sig': hexs(sig_raw), 'valid': False,
         'expect': 'error.guard.signature',
         'why': 'getrennte Zweckzeichenfolgen; ohne sie waere dieser Fall '
                'gueltig und die Bindung an die Programmdatei weg'},
        {'name': 'eine Auftragsunterschrift als Praesenzantwort',
         'nonce': hexs(nonce_a), 'binhash': hexs(program),
         'pub': hexs(pub), 'sig': hexs(sig_job), 'valid': False,
         'expect': 'error.guard.signature',
         'why': 'der Zweck steht vorn, und das Geraet setzt ihn'},
    ]

    out = {
        'kommentar': ('Aufgezeichnete Antworten des Geraets. Erzeugt von '
                      'make-replay-vectors.py, unterschrieben mit '
                      'tools/ed25519.py. Die Bibliothek baut die Botschaft '
                      'selbst: <zweck> 0x00 nonce(32) binhash(32).'),
        'domain': ATTEST.decode('ascii'),
        'device_pub': hexs(pub),
        'other_pub': hexs(other_pub),
        'cases': cases,
    }
    path = os.path.join(HERE, 'replay-vectors.json')
    with io.open(path, 'w', encoding='utf-8', newline='\n') as handle:
        json.dump(out, handle, indent=2, ensure_ascii=True, sort_keys=False)
        handle.write('\n')
    sys.stdout.write('%s: %d Faelle, %d gueltig\n'
                     % (os.path.basename(path), len(cases),
                        sum(1 for c in cases if c['valid'])))
    return 0


if __name__ == '__main__':
    sys.exit(main())
