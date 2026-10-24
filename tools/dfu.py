# -*- coding: utf-8 -*-
"""Firmware-Update aufspielen - vom Programm aus und von Hand.

    python tools/dfu.py dist/osk-0.0.1.0-osk_v1.oskfw            # aufspielen
    python tools/dfu.py dist/osk-...oskfw --dry-run              # nur pruefen
    python tools/dfu.py dist/osk-...oskfw --manual               # von Hand
    python tools/dfu.py --info dist/osk-...oskfw                 # was ist drin
    python tools/dfu.py --state                                  # was laeuft gerade

DER STICK FRAGT VORHER. Jedes Aufspielen braucht einen Tastendruck am
Geraet; das Werkzeug kann das nicht umgehen und versucht es auch nicht. Was
hier passiert, ist: pruefen, uebertragen, Umschalten erbitten. Entschieden
wird am Geraet.

Gesprochen wird Befehl 13 des Protokolls (shared/protocol.md), in CBOR in
64-Byte-Berichten. Die Unterbefehle stehen in docs/update.md.

UNGEPRUEFT: Es gibt kein Geraet. Der Weg ueber USB-HID ist nie gelaufen -
er folgt dem Protokoll, nicht einer Messung. Was gemessen ist: der Ablauf
gegen eine Gegenstelle, die das Geraet nachbildet (tools/tests/test_dfu.py),
und das Ablehnen beschaedigter Pakete, bevor ein Byte uebertragen wird.

Nur Standardbibliothek.
"""
import argparse
import binascii
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import common
import sign

PROTOCOL_VERSION = 1
CMD_UPDATE = 13

OP_BEGIN = 1
OP_CHUNK = 2
OP_FINISH = 3
OP_ABORT = 4
OP_STATE = 5

# So viel Abbild passt neben Kopf und Rahmen in einen Bericht. Klein, weil
# ein HID-Bericht 64 Byte hat und das Geraet die Kette selbst wieder
# zusammensetzt; die Zahl ist eine Vorgabe, keine Messung.
CHUNK = 512

STAGES = {0: 'nichts im Gange', 1: 'wird geschrieben', 2: 'bereit zum Umschalten',
          3: 'umgeschaltet, Bestaetigung fehlt', 4: 'bestaetigt'}


class DfuError(Exception):
    """Mit einem Satz, den ein Mensch lesen kann."""


# --- CBOR, nur was gebraucht wird ------------------------------------------
#
# Ganze Zahlen, Byteketten, Text und Abbildungen. Kein Fremdcode: das ist
# RFC 8949 Abschnitt 3, und mehr als diese vier Sorten kommt im Protokoll
# nicht vor.

def _head(major, value):
    if value < 24:
        return bytes([(major << 5) | value])
    if value < 0x100:
        return bytes([(major << 5) | 24, value])
    if value < 0x10000:
        return bytes([(major << 5) | 25]) + value.to_bytes(2, 'big')
    if value < 0x100000000:
        return bytes([(major << 5) | 26]) + value.to_bytes(4, 'big')
    return bytes([(major << 5) | 27]) + value.to_bytes(8, 'big')


def cbor_dump(value):
    if isinstance(value, bool):
        raise DfuError('Wahrheitswerte kommen im Protokoll nicht vor')
    if isinstance(value, int):
        if value < 0:
            raise DfuError('negative Zahlen kommen im Protokoll nicht vor')
        return _head(0, value)
    if isinstance(value, bytes):
        return _head(2, len(value)) + value
    if isinstance(value, str):
        raw = value.encode('utf-8')
        return _head(3, len(raw)) + raw
    if isinstance(value, dict):
        # Schluessel der Groesse nach - so ist dieselbe Abbildung immer
        # dieselbe Bytekette, und das Geraet kann sie wiedererkennen.
        items = sorted(value.items(), key=lambda kv: cbor_dump(kv[0]))
        out = _head(5, len(items))
        for key, inner in items:
            out += cbor_dump(key) + cbor_dump(inner)
        return out
    raise DfuError('Sorte kommt im Protokoll nicht vor: %r' % type(value))


def _load(data, pos):
    if pos >= len(data):
        raise DfuError('Antwort bricht mitten ab')
    first = data[pos]
    major, extra = first >> 5, first & 0x1F
    pos += 1
    if extra < 24:
        value = extra
    elif extra in (24, 25, 26, 27):
        width = {24: 1, 25: 2, 26: 4, 27: 8}[extra]
        # Abgeschnitten ist abgeschnitten: int.from_bytes nimmt auch zu
        # wenige Byte an und liefert stillschweigend eine falsche Zahl.
        if pos + width > len(data):
            raise DfuError('Antwort bricht mitten ab')
        value, pos = int.from_bytes(data[pos:pos + width], 'big'), pos + width
    else:
        raise DfuError('unbekannte Laengenangabe in der Antwort')

    if major == 0:
        return value, pos
    if major in (2, 3):
        if pos + value > len(data):
            raise DfuError('Antwort bricht mitten ab')
        raw = data[pos:pos + value]
        return (raw if major == 2 else raw.decode('utf-8', 'replace')), pos + value
    if major == 5:
        out = {}
        for _ in range(value):
            key, pos = _load(data, pos)
            inner, pos = _load(data, pos)
            out[key] = inner
        return out, pos
    raise DfuError('Sorte %d kommt im Protokoll nicht vor' % major)


def cbor_load(data):
    value, _ = _load(data, 0)
    return value


# --- Eine Aufspielung ------------------------------------------------------

class Device:
    """Die Gegenstelle. `send` nimmt eine Bytekette und gibt eine zurueck -
    genau einmal je Nachricht. Wie die Bytes zum Geraet kommen (USB-HID,
    BLE, eine Attrappe im Test), weiss diese Klasse nicht."""

    def __init__(self, send):
        self.send = send
        self.sequence = 0

    def call(self, data):
        self.sequence += 1
        frame = {'v': PROTOCOL_VERSION, 'c': CMD_UPDATE, 's': self.sequence, 'd': data}
        answer = cbor_load(self.send(cbor_dump(frame)))
        if not isinstance(answer, dict):
            raise DfuError('Das Geraet antwortet nicht in der erwarteten Form.')
        if answer.get('s') != self.sequence:
            raise DfuError('Antwort gehoert zu einer anderen Frage (%r statt %d).'
                           % (answer.get('s'), self.sequence))
        result = answer.get('r', -1)
        if result != 0:
            raise DfuError('Das Geraet lehnt ab: %s' % result_text(result))
        return answer.get('d') or {}


# Dieselbe Reihenfolge wie osk_result_t in apps/firmware/include/osk_result.h.
RESULTS = {
    0: 'in Ordnung',
    1: 'Aufrufparameter unbrauchbar',
    2: 'Leitung oder Flash gestoert',
    3: 'Zeit abgelaufen',
    4: 'Freigabe fehlt - die Taste am Geraet wurde nicht gedrueckt',
    5: 'Geraet gesperrt',
    6: 'nicht gefunden',
    7: 'Rueckschritt: diese Fassung ist nicht neuer als die laufende',
    8: 'kein Platz: das Abbild passt nicht in den zweiten Platz',
    9: 'Aufbau passt nicht - Paket beschaedigt oder Hash falsch',
    10: 'das Geraet kennt diesen Befehl nicht, oder das Abbild ist fuer ein anderes Brett',
    11: 'interner Fehler im Geraet',
}


def result_text(code):
    return RESULTS.get(code, 'unbekannter Grund (%r)' % code)


def upload(device, package, chunk=CHUNK, progress=None):
    """Paket aufspielen. Gibt die Zahl der uebertragenen Byte zurueck.

    Reihenfolge wie in apps/firmware/boot/boot_session.c: erst Kopf und
    Signatur, dann das Abbild der Reihe nach, dann abschliessen. Bricht
    etwas ab, bleibt die alte Fassung auf dem Geraet - das Werkzeug muss
    dafuer nichts tun und kann es auch nicht verderben."""
    info = sign.unpack(package)
    head = package[:sign.HEADER_LEN + sign.SIG_LEN]
    image = info['image']

    device.call({'op': OP_BEGIN, 'hdr': head})
    sent = 0
    while sent < len(image):
        part = image[sent:sent + chunk]
        device.call({'op': OP_CHUNK, 'off': sent, 'b': part})
        sent += len(part)
        if progress:
            progress(sent, len(image))
    device.call({'op': OP_FINISH})
    return sent


def read_state(device):
    answer = device.call({'op': OP_STATE})
    stage = answer.get('stage', 0)
    return {
        'stage': stage,
        'stage_text': STAGES.get(stage, 'unbekannt (%r)' % stage),
        'written': answer.get('written', 0),
        'image_len': answer.get('len', 0),
        'version': answer.get('ver', ''),
    }


def abort(device):
    return device.call({'op': OP_ABORT})


# --- Wege zum Geraet -------------------------------------------------------

def hid_transport():
    """USB-HID. UNGEPRUEFT - es gibt kein Geraet und keine Bibliothek dafuer
    in diesem Projekt. Statt etwas zu behaupten, sagt die Funktion klar,
    was fehlt."""
    raise DfuError(
        'Der Weg ueber USB-HID ist in diesem Werkzeug noch nicht gebaut.\n'
        'Es gab beim Schreiben kein Geraet, und ein ungepruefter Weg, der\n'
        'aussieht als ginge er, ist schlimmer als keiner.\n'
        '\n'
        'Was heute geht:\n'
        '  python tools/dfu.py <Paket> --manual     Schritte von Hand\n'
        '  python tools/dfu.py <Paket> --dry-run    Paket pruefen\n'
        '\n'
        'Was fehlt: eine Verbindung zum Geraet (siehe docs/update.md,\n'
        'Abschnitt "Was noch fehlt").')


def manual_steps(path, info):
    return '\n'.join([
        'Von Hand aufspielen - %s' % os.path.basename(path),
        '',
        '  Brett:    %s' % info['board'],
        '  Fassung:  %s' % info['version'],
        '  Zaehler:  %d' % info['counter'],
        '  Abbild:   %d Byte' % info['image_len'],
        '',
        'Schritte:',
        ' 1. Stick abziehen.',
        ' 2. Taste gedrueckt halten und einstecken - der Bootloader meldet sich.',
        ' 3. Das Verwaltungsprogramm oeffnen, "Firmware" waehlen und diese Datei',
        '    angeben:',
        '      %s' % os.path.abspath(path),
        ' 4. Am Geraet die Taste druecken, wenn das Display danach fragt.',
        ' 5. Der Stick startet neu. Meldet sich die neue Fassung nicht, dreht er',
        '    nach drei Versuchen von selbst auf die alte zurueck - ohne Zutun.',
        '',
        'Geht dabei etwas schief, laeuft die alte Fassung weiter. Der Tresor',
        'wird nicht angefasst.',
    ])


# --- Aufruf ----------------------------------------------------------------

def describe(path):
    with io.open(path, 'rb') as handle:
        data = handle.read()
    info = sign.unpack(data)
    lines = ['%s' % os.path.abspath(path),
             '  Brett:    %s (%d)' % (info['board'], info['board_id']),
             '  Fassung:  %s' % info['version'],
             '  Zaehler:  %d' % info['counter'],
             '  Abbild:   %d Byte' % info['image_len'],
             '  SHA-256:  %s' % binascii.hexlify(info['image_hash']).decode('ascii')]
    key = sign.key_path(None)
    if os.path.isfile(key):
        try:
            pub = sign.ed25519.public_key(sign.read_key(key))
            good, text = sign.verify(data, pub)
            lines.append('  Signatur: %s' % text)
        except sign.SignError as error:
            lines.append('  Signatur: nicht geprueft (%s)' % error)
    else:
        lines.append('  Signatur: nicht geprueft - kein oeffentlicher Schluessel hier')
    return '\n'.join(lines), info


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('package', nargs='?', help='Paket aus tools/sign.py')
    parser.add_argument('--manual', action='store_true', help='Schritte von Hand ausgeben')
    parser.add_argument('--dry-run', action='store_true', help='nur pruefen, nichts senden')
    parser.add_argument('--info', metavar='PAKET', help='was in einem Paket steht')
    parser.add_argument('--state', action='store_true', help='Zustand am Geraet lesen')
    parser.add_argument('--abort', action='store_true', help='laufende Aufspielung abbrechen')
    parser.add_argument('--chunk', type=int, default=CHUNK)
    args = parser.parse_args(argv)

    try:
        if args.info:
            text, _ = describe(args.info)
            print(text)
            return 0
        if args.state or args.abort:
            hid_transport()  # sagt, was fehlt
            return 1
        if not args.package:
            parser.error('ein Paket angeben, oder --info / --state')
        if args.chunk < 1 or args.chunk > 4096:
            parser.error('--chunk muss zwischen 1 und 4096 liegen')

        text, info = describe(args.package)
        print(text)
        if args.manual:
            print()
            print(manual_steps(args.package, info))
            return 0
        if args.dry_run:
            print('\n--dry-run: nichts gesendet.')
            return 0
        hid_transport()
        return 1
    except (DfuError, sign.SignError) as error:
        sys.stderr.write('%s\n' % error)
        return 1
    except OSError as error:
        sys.stderr.write('%s\n' % error)
        return 1


if __name__ == '__main__':
    sys.exit(main())
