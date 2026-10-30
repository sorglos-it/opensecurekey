# -*- coding: utf-8 -*-
"""Die zwei Firmware-Fassungen auseinanderhalten: mit und ohne RSA.

    python tools/variant.py --table
    python tools/variant.py --check dist/osk-firmware-0.0.1.0-nrf52840dk-norsa.bin
    python tools/variant.py --check <abbild> --expect rsa
    python tools/variant.py --selftest

WOGEGEN DAS HIER SCHUETZT - und es ist genau ein Fall:

  Ein Abbild OHNE RSA, das den AAGUID der Fassung MIT RSA traegt. Dann
  verspricht das Geraet dem Browser Faehigkeiten, die es nicht hat. FIDO
  verlangt, dass ein AAGUID eine Fassung mit DENSELBEN Faehigkeiten
  bezeichnet (docs/ctap.md); zwei Fassungen brauchen darum zwei AAGUIDs.
  Verwechselt sie jemand, faellt es ohne diese Pruefung erst beim Nutzer
  auf - als Anmeldung, die scheitert, oder als Schluessel, der nie entsteht.

DIE DREI ORTE, DIE UEBEREINSTIMMEN MUESSEN:

  1. Die Option im Bau      CONFIG_OSK_RSA  (apps/firmware/Kconfig)
  2. Der Dateiname          ...-rsa.bin / ...-norsa.bin
  3. Der Abbildkopf         AAGUID als geschuetztes TLV im MCUboot-Kopf

Stimmen sie nicht, ist das ein FEHLER und keine Warnung. Ein Abbild, dessen
drei Orte sich widersprechen, gehoert in kein Geraet.

EINE ZAHL NUR AN EINER STELLE: die beiden AAGUIDs stehen nicht hier, sondern
in apps/firmware/src/classes/ctap/osk_ctap_core.c - dort liest sie der
Quelltext, und dort liest sie dieses Werkzeug. Wer sie hier noch einmal
hinschriebe, haette zwei Wahrheiten und irgendwann zwei verschiedene.

Nur Standardbibliothek.
"""
import argparse
import binascii
import hashlib
import io
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import common

VARIANTS = ('rsa', 'norsa')

# Wo die beiden AAGUIDs stehen - die einzige Stelle.
AAGUID_SOURCE = os.path.join('apps', 'firmware', 'src', 'classes', 'ctap', 'osk_ctap_core.c')
AAGUID_LEN = 16

# Welcher C-Name zu welcher Fassung gehoert. Das sagt die Weiche in
# osk_ctap_core.c: #if defined(CONFIG_OSK_RSA) -> OSK_AAGUID_RSA.
AAGUID_NAMES = {'rsa': 'OSK_AAGUID_RSA', 'norsa': 'OSK_AAGUID_NO_RSA'}

# --- MCUboot-Abbildkopf ------------------------------------------------------
#
# Das Format ist das von MCUboot, nicht unseres: MCUboot liest den Kopf, also
# wird er hier nur GELESEN und nichts erfunden. Aufbau nach image.h von
# MCUboot (32 Byte), danach die TLV-Bereiche.
#
# GESCHUETZT heisst: von der Signatur gedeckt. Der AAGUID gehoert dorthin und
# nicht in den ungeschuetzten Bereich - sonst tauscht ihn jemand aus, ohne die
# Signatur zu brechen, und die ganze Pruefung hier waere eine Hoeflichkeit.
IMAGE_MAGIC = 0x96F3B83D
IMAGE_HEADER_LEN = 32
TLV_INFO_MAGIC = 0x6907          # ungeschuetzt
TLV_PROT_INFO_MAGIC = 0x6908     # geschuetzt, von der Signatur gedeckt
TLV_INFO_LEN = 4

# Eigenes TLV im Bereich, den MCUboot den Anwendungen laesst (ab 0x00a0).
# MCUboot selbst kennt den Wert nicht und reicht ihn durch.
TLV_OSK_AAGUID = 0x00A1


class VariantError(Exception):
    """Etwas stimmt nicht - mit einem Satz, den ein Mensch lesen kann."""


# --- Die Tafel: Fassung -> AAGUID -------------------------------------------

def read_aaguid_table(root=common.ROOT):
    """Liest die beiden AAGUIDs aus osk_ctap_core.c. Gibt {Fassung: bytes}.

    Gelesen wird der Quelltext, nicht eine Kopie davon. Fehlt einer der
    beiden Namen oder hat er nicht 16 Byte, ist das ein Fehler und keine
    leere Tafel: eine leere Tafel liesse jede Pruefung unten durchgehen.
    """
    path = os.path.join(root, AAGUID_SOURCE)
    try:
        with io.open(path, encoding='utf-8') as handle:
            text = handle.read()
    except OSError as error:
        raise VariantError('Die AAGUID-Tafel ist nicht lesbar: %s' % error)
    table = {}
    for variant, name in sorted(AAGUID_NAMES.items()):
        match = re.search(r'\b%s\s*\[\s*(\d*)\s*\]\s*=\s*\{(.*?)\}' % re.escape(name),
                          text, re.S)
        if not match:
            raise VariantError('%s steht nicht in %s - ohne beide Werte kann hier nichts '
                               'geprueft werden.' % (name, AAGUID_SOURCE))
        declared = match.group(1).strip()
        numbers = [int(n, 0) for n in re.findall(r'0x[0-9a-fA-F]+|\b\d+\b', match.group(2))]
        if declared and int(declared) != AAGUID_LEN:
            raise VariantError('%s ist als [%s] erklaert, ein AAGUID hat %d Byte.'
                               % (name, declared, AAGUID_LEN))
        if len(numbers) != AAGUID_LEN:
            raise VariantError('%s hat %d Byte, ein AAGUID hat %d.'
                               % (name, len(numbers), AAGUID_LEN))
        if any(n < 0 or n > 0xFF for n in numbers):
            raise VariantError('%s enthaelt einen Wert, der in kein Byte passt.' % name)
        table[variant] = bytes(bytearray(numbers))
    if table['rsa'] == table['norsa']:
        raise VariantError('Beide Fassungen haben denselben AAGUID. Dann ist die Fassung ohne '
                           'RSA von der mit RSA nicht zu unterscheiden - genau der Fall, gegen '
                           'den diese Pruefung da ist.')
    return table


def aaguid_for(variant, root=common.ROOT):
    if variant not in VARIANTS:
        raise VariantError('Unbekannte Fassung %r - es gibt %s.'
                           % (variant, ' und '.join(VARIANTS)))
    return read_aaguid_table(root)[variant]


def variant_for_aaguid(value, root=common.ROOT):
    """Welche Fassung traegt diesen AAGUID? None, wenn keine."""
    for variant, known in sorted(read_aaguid_table(root).items()):
        if known == value:
            return variant
    return None


# --- Die Fassung aus einer Baudatei oder aus dem fertigen Bau ---------------

CONFIG_RE = re.compile(r'^\s*CONFIG_OSK_RSA\s*=\s*(y|n)\s*$', re.M)
CONFIG_OFF_RE = re.compile(r'^\s*#\s*CONFIG_OSK_RSA\s+is not set\s*$', re.M)


def variant_from_config(text):
    """Fassung aus einer .conf oder einer fertigen .config. (Fassung, Begruendung).

    Drei Faelle, und alle drei werden unterschieden: ausdrueckliches y,
    ausdrueckliches n, und "steht nicht drin". Der dritte ist NICHT dasselbe
    wie n: wer die Zeile vergisst, bekommt von Kconfig still die Vorgabe,
    und dann weiss niemand, ob die Fassung gewollt oder gestolpert ist.
    Darum sagt dieses Werkzeug dann (None, Grund) - und der Aufrufer
    entscheidet, ob er damit leben will.
    """
    match = CONFIG_RE.search(text or '')
    if match:
        return ('rsa' if match.group(1) == 'y' else 'norsa',
                'CONFIG_OSK_RSA=%s steht ausdruecklich da' % match.group(1))
    if CONFIG_OFF_RE.search(text or ''):
        # So schreibt Kconfig ein n in die fertige .config.
        return 'norsa', 'CONFIG_OSK_RSA is not set - so schreibt Kconfig ein n'
    return None, 'CONFIG_OSK_RSA steht nirgends - die Fassung ist nicht festgelegt'


def variant_from_build(build_dir):
    """Fassung aus dem WIRKLICH gebauten Bau: die .config, die Kconfig schrieb.

    Das ist die belastbarste Quelle, die es gibt - .conf-Dateien sagen, was
    gewuenscht war, die .config sagt, was herauskam. Eine Zeile, die Kconfig
    still verworfen hat (siehe apps/firmware/prj.conf), steht in der .conf,
    aber nicht in der .config.
    """
    found = []
    for base, dirs, files in os.walk(build_dir):
        dirs[:] = [d for d in dirs if d != 'CMakeFiles']
        if '.config' in files:
            found.append(os.path.join(base, '.config'))
    for path in sorted(found, key=lambda p: (p.count(os.sep), p)):
        with io.open(path, encoding='utf-8', errors='replace') as handle:
            variant, why = variant_from_config(handle.read())
        if variant:
            return variant, '%s: %s' % (os.path.relpath(path, build_dir), why)
    if not found:
        return None, 'keine .config in %s - es wurde nichts gebaut' % build_dir
    return None, 'CONFIG_OSK_RSA steht in keiner .config unter %s' % build_dir


# --- Die Fassung aus dem Dateinamen ------------------------------------------

def variant_from_name(name):
    """Fassung aus dem Dateinamen. (Fassung, Begruendung).

    Auf -norsa wird VOR -rsa geprueft: norsa enthaelt rsa, und wer erst nach
    rsa sucht, haelt jedes Abbild ohne RSA fuer eines mit.
    """
    stem = os.path.basename(name).lower()
    for marker, variant in (('-norsa', 'norsa'), ('-rsa', 'rsa')):
        if marker in stem:
            return variant, 'der Dateiname sagt %s' % marker.lstrip('-')
    return None, 'im Dateinamen steht keine Fassung (erwartet -rsa oder -norsa)'


def name_for(version, board, variant, suffix):
    """Der Dateiname einer Fassung - an einer Stelle gebildet, nicht an drei."""
    if variant not in VARIANTS:
        raise VariantError('Unbekannte Fassung %r.' % (variant,))
    return 'osk-firmware-%s-%s-%s%s' % (version, board, variant, suffix)


# --- Die Fassung aus dem Abbildkopf -----------------------------------------

def read_image_header(data):
    """Liest den MCUboot-Abbildkopf. Gibt ein dict oder wirft VariantError."""
    if len(data) < IMAGE_HEADER_LEN:
        raise VariantError('Zu kurz fuer einen Abbildkopf: %d Byte, mindestens %d.'
                           % (len(data), IMAGE_HEADER_LEN))
    magic, load_addr, hdr_size, prot_size, img_size, flags = struct.unpack('<IIHHII', data[:20])
    if magic != IMAGE_MAGIC:
        raise VariantError('Kein MCUboot-Abbild (Kennung %#010x statt %#010x). Ein rohes '
                           'zephyr.bin ohne Kopf ist noch nicht unterschrieben.'
                           % (magic, IMAGE_MAGIC))
    major, minor, revision, build_num = struct.unpack('<BBHI', data[20:28])
    if hdr_size < IMAGE_HEADER_LEN:
        raise VariantError('Der Kopf sagt, er sei %d Byte lang - weniger als %d geht nicht.'
                           % (hdr_size, IMAGE_HEADER_LEN))
    return {'load_addr': load_addr, 'hdr_size': hdr_size, 'prot_tlv_size': prot_size,
            'img_size': img_size, 'flags': flags,
            'version': '%d.%d.%d.%d' % (major, minor, revision, build_num)}


def read_tlvs(data, head):
    """Alle TLVs des Abbilds. Gibt eine Liste (Typ, Wert, geschuetzt).

    Zwei Bereiche koennen da sein: zuerst der GESCHUETZTE (0x6908), dann der
    ungeschuetzte (0x6907). Wo ein TLV steht, entscheidet, was es wert ist -
    darum wird das mitgegeben und nicht eingeschmolzen.
    """
    out = []
    pos = head['hdr_size'] + head['img_size']
    for expect, protected in ((TLV_PROT_INFO_MAGIC, True), (TLV_INFO_MAGIC, False)):
        if pos + TLV_INFO_LEN > len(data):
            break
        magic, total = struct.unpack('<HH', data[pos:pos + TLV_INFO_LEN])
        if magic != expect:
            continue
        if total < TLV_INFO_LEN or pos + total > len(data):
            raise VariantError('Der TLV-Bereich bei %#x sagt %d Byte, das Abbild hat dort nur %d.'
                               % (pos, total, len(data) - pos))
        end, at = pos + total, pos + TLV_INFO_LEN
        while at + 4 <= end:
            tlv_type, tlv_len = struct.unpack('<HH', data[at:at + 4])
            if at + 4 + tlv_len > end:
                raise VariantError('Ein TLV bei %#x reicht ueber seinen Bereich hinaus.' % at)
            out.append((tlv_type, data[at + 4:at + 4 + tlv_len], protected))
            at += 4 + tlv_len
        pos = end
    return out


def aaguid_from_image(data):
    """Der AAGUID aus dem Abbildkopf. (AAGUID oder None, Begruendung).

    Nur ein GESCHUETZTES TLV gilt. Steht der AAGUID im ungeschuetzten
    Bereich, ist er austauschbar, ohne die Signatur zu brechen - und ein
    austauschbarer AAGUID ist keiner. Darum ist das hier ein Fehler und
    nicht nah genug.
    """
    head = read_image_header(data)
    loose = None
    for tlv_type, value, protected in read_tlvs(data, head):
        if tlv_type != TLV_OSK_AAGUID:
            continue
        if not protected:
            loose = value
            continue
        if len(value) != AAGUID_LEN:
            raise VariantError('Das AAGUID-TLV im Abbildkopf hat %d Byte, ein AAGUID hat %d.'
                               % (len(value), AAGUID_LEN))
        return value, 'geschuetztes TLV %#06x im Abbildkopf' % TLV_OSK_AAGUID
    if loose is not None:
        raise VariantError('Der AAGUID steht im UNGESCHUETZTEN TLV-Bereich. Dort ist er '
                           'austauschbar, ohne die Signatur zu brechen - er gehoert in den '
                           'geschuetzten Bereich (imgtool: --custom-tlv, siehe docs/signatur.md).')
    return None, 'kein AAGUID-TLV %#06x im Abbildkopf' % TLV_OSK_AAGUID


def build_image(image, variant, version='0.0.1.0', root=common.ROOT, aaguid=None,
                protected=True):
    """Baut ein Abbild im MCUboot-Format MIT AAGUID-TLV - fuer Pruefungen.

    Das ist KEIN Ersatz fuer imgtool: hier wird nichts unterschrieben, und
    das Ergebnis gehoert in kein Geraet. Es gibt dieses Stueck nur, damit die
    Pruefung oben ohne nRF Connect SDK an echten Bytes gemessen werden kann -
    samt dem Fall, gegen den sie da ist (falscher AAGUID). Ohne das waere die
    Pruefung nur geschrieben und nicht gemessen.
    """
    if aaguid is None:
        aaguid = aaguid_for(variant, root)
    numbers = common.parse_version(version)
    head = struct.pack('<IIHHII', IMAGE_MAGIC, 0, IMAGE_HEADER_LEN,
                       4 + 4 + len(aaguid) if protected else 0, len(image), 0)
    head += struct.pack('<BBHI', numbers[0], numbers[1], numbers[2], numbers[3])
    head += struct.pack('<I', 0)
    assert len(head) == IMAGE_HEADER_LEN, len(head)
    tlv = struct.pack('<HH', TLV_OSK_AAGUID, len(aaguid)) + aaguid
    magic = TLV_PROT_INFO_MAGIC if protected else TLV_INFO_MAGIC
    return head + image + struct.pack('<HH', magic, TLV_INFO_LEN + len(tlv)) + tlv


# --- Die Pruefung -----------------------------------------------------------

def check(path, expect=None, build_dir=None, root=common.ROOT):
    """Stimmen Dateiname, Abbildkopf und Bau ueberein? (ok, Zeilen).

    Alle Quellen werden angesehen, bevor geurteilt wird - eine Pruefung, die
    beim ersten Verdacht abbricht, nennt nur den ersten von drei Fehlern.
    """
    lines, found = [], {}
    table = read_aaguid_table(root)
    lines.append('AAGUID-Tafel aus %s: rsa %s, norsa %s'
                 % (AAGUID_SOURCE, binascii.hexlify(table['rsa']).decode('ascii'),
                    binascii.hexlify(table['norsa']).decode('ascii')))

    variant, why = variant_from_name(path)
    lines.append('Dateiname: %s (%s)' % (variant or 'keine Fassung', why))
    if variant:
        found['Dateiname'] = variant
    else:
        # KEIN Durchlassen. Gemessen am 30.10.2026: ohne diesen Riegel ging ein
        # abbild namens "zephyr.bin" mit gueltigem AAGUID durch, weil dann nur
        # eine Quelle uebrig war - und eine einzige Quelle stimmt immer mit
        # sich selbst ueberein. Die Fassung muss im Dateinamen UND im
        # Abbildkopf stehen; fehlt eine der beiden, ist nichts verglichen.
        return False, lines + [
            'FEHLER: Im Dateinamen steht keine Fassung. Dann gibt es nichts zu vergleichen - '
            'wer das Abbild weitergibt, weiss nicht, was er weitergibt. Erwartet wird ein Name '
            'wie %s.' % name_for('0.0.1.0', 'nrf52840dk', 'norsa', '.bin')]

    try:
        with io.open(path, 'rb') as handle:
            data = handle.read()
    except OSError as error:
        return False, lines + ['FEHLER: %s' % error]

    try:
        value, why = aaguid_from_image(data)
    except VariantError as error:
        return False, lines + ['FEHLER im Abbildkopf: %s' % error]
    if value is None:
        lines.append('Abbildkopf: %s' % why)
        return False, lines + [
            'FEHLER: Das Abbild sagt selbst nicht, welche Fassung es ist. Dann entscheidet '
            'allein der Dateiname - und ein Dateiname ist umbenannt, bevor man zweimal '
            'hinsieht. Siehe docs/signatur.md.']
    from_image = variant_for_aaguid(value, root)
    lines.append('Abbildkopf: AAGUID %s -> %s (%s)'
                 % (binascii.hexlify(value).decode('ascii'),
                    from_image or 'KEINE BEKANNTE FASSUNG', why))
    if from_image is None:
        return False, lines + [
            'FEHLER: Dieser AAGUID gehoert zu keiner der beiden Fassungen. Entweder ist er '
            'von Hand eingetragen worden, oder die Tafel in %s ist weitergezogen und das '
            'Abbild ist alt.' % AAGUID_SOURCE]
    found['Abbildkopf'] = from_image

    if build_dir:
        from_build, why = variant_from_build(build_dir)
        lines.append('Bau: %s (%s)' % (from_build or 'nicht festgelegt', why))
        if from_build:
            found['Bau'] = from_build
        else:
            lines.append('WARNUNG: Der Bau sagt die Fassung nicht - geprueft sind nur die '
                         'anderen Quellen.')

    if expect:
        if expect not in VARIANTS:
            return False, lines + ['FEHLER: --expect %s kennt niemand.' % expect]
        found['--expect'] = expect

    sides = sorted(set(found.values()))
    if len(sides) > 1:
        lines.append('FEHLER: Die Fassung ist nicht ueberall dieselbe - '
                     + ', '.join('%s sagt %s' % (k, v) for k, v in sorted(found.items())))
        if found.get('Abbildkopf') == 'rsa' and 'norsa' in sides:
            lines.append('GENAU DER GEFAEHRLICHE FALL: ein Abbild ohne RSA mit dem AAGUID der '
                         'Fassung mit RSA. So verspricht das Geraet Faehigkeiten, die es nicht '
                         'hat. Dieses Abbild gehoert in kein Geraet.')
        return False, lines
    lines.append('SHA-256 des Abbilds: %s' % hashlib.sha256(data).hexdigest())
    lines.append('In Ordnung: alle %d Quellen sagen %s.' % (len(found), sides[0]))
    return True, lines


# --- Selbstpruefung ---------------------------------------------------------

def selftest(root=common.ROOT):
    """(Zahl der Fehler, Zeilen). Prueft BEIDE Richtungen und den Fehlerfall."""
    wrong, lines = 0, []

    def case(name, got, want):
        same = got == want
        lines.append('%-58s %s' % (name, 'richtig' if same else 'FALSCH (%r)' % (got,)))
        return 0 if same else 1

    table = read_aaguid_table(root)
    wrong += case('Tafel: zwei verschiedene AAGUIDs', table['rsa'] != table['norsa'], True)
    wrong += case('Tafel: beide 16 Byte', (len(table['rsa']), len(table['norsa'])), (16, 16))

    # Beide Richtungen - eine Option, die nur in einer Richtung geprueft ist,
    # ist keine.
    for variant in VARIANTS:
        wrong += case('Fassung %s: Dateiname wird wiedererkannt' % variant,
                      variant_from_name(name_for('0.0.1.0', 'nrf52840dk', variant, '.bin'))[0],
                      variant)
        wrong += case('Fassung %s: AAGUID wird wiedererkannt' % variant,
                      variant_for_aaguid(aaguid_for(variant, root), root), variant)
        data = build_image(b'\x11' * 64, variant, root=root)
        wrong += case('Fassung %s: AAGUID aus dem Abbildkopf' % variant,
                      aaguid_from_image(data)[0], table[variant])

    wrong += case('-norsa wird nicht als -rsa gelesen',
                  variant_from_name('osk-firmware-0.0.1.0-nrf52840dk-norsa.bin')[0], 'norsa')
    wrong += case('ohne Fassung im Namen: keine Fassung',
                  variant_from_name('zephyr.bin')[0], None)

    wrong += case('Baudatei y -> rsa', variant_from_config('CONFIG_OSK_RSA=y\n')[0], 'rsa')
    wrong += case('Baudatei n -> norsa', variant_from_config('CONFIG_OSK_RSA=n\n')[0], 'norsa')
    wrong += case('fertige .config is-not-set -> norsa',
                  variant_from_config('# CONFIG_OSK_RSA is not set\n')[0], 'norsa')
    wrong += case('Zeile fehlt: NICHT als n gelesen',
                  variant_from_config('CONFIG_LOG=y\n')[0], None)

    # Der Fall, gegen den das Ganze da ist: Abbild ohne RSA, AAGUID mit RSA.
    data = build_image(b'\x22' * 64, 'norsa', root=root, aaguid=table['rsa'])
    wrong += case('VERWECHSLUNG: norsa-Abbild mit rsa-AAGUID wird erkannt',
                  variant_for_aaguid(aaguid_from_image(data)[0], root), 'rsa')

    # Ein AAGUID im ungeschuetzten Bereich muss auffallen.
    data = build_image(b'\x33' * 64, 'rsa', root=root, protected=False)
    try:
        aaguid_from_image(data)
        lines.append('%-58s FALSCH (durchgelassen)' % 'ungeschuetztes AAGUID-TLV faellt auf')
        wrong += 1
    except VariantError:
        lines.append('%-58s richtig' % 'ungeschuetztes AAGUID-TLV faellt auf')

    # Ein rohes zephyr.bin ohne Kopf.
    try:
        read_image_header(b'\x00' * 64)
        lines.append('%-58s FALSCH (durchgelassen)' % 'Abbild ohne MCUboot-Kopf faellt auf')
        wrong += 1
    except VariantError:
        lines.append('%-58s richtig' % 'Abbild ohne MCUboot-Kopf faellt auf')

    return wrong, lines


# --- Aufruf -----------------------------------------------------------------

def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--table', action='store_true', help='die beiden AAGUIDs zeigen')
    parser.add_argument('--check', metavar='ABBILD',
                        help='Dateiname, Abbildkopf und Bau vergleichen')
    parser.add_argument('--expect', choices=VARIANTS, help='welche Fassung erwartet wird')
    parser.add_argument('--build-dir', help='Bauordner, dessen .config mitgeprueft wird')
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args(argv)
    try:
        if args.selftest:
            wrong, lines = selftest(root)
            common.say('\n'.join(lines))
            common.say('%d von %d falsch.' % (wrong, len(lines)))
            return 1 if wrong else 0
        if args.table:
            for variant, value in sorted(read_aaguid_table(root).items()):
                common.say('%-6s %s  (%s)' % (variant, binascii.hexlify(value).decode('ascii'),
                                              AAGUID_NAMES[variant]))
            return 0
        if args.check:
            good, lines = check(args.check, args.expect, args.build_dir, root)
            common.say('\n'.join(lines))
            return 0 if good else 1
        parser.error('--table, --check oder --selftest angeben')
    except VariantError as error:
        sys.stderr.write('%s\n' % error)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
