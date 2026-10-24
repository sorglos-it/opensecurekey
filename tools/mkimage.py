# -*- coding: utf-8 -*-
"""Das Abbild des schreibgeschuetzten Laufwerks bauen, das der Stick mitbringt.

    python tools/mkimage.py --windows dist/osk-windows.exe \\
                            --linux   dist/osk-linux \\
                            --macos   dist/osk-macos \\
                            --out     dist/laufwerk.img
    python tools/mkimage.py --check dist/laufwerk.img
    python tools/mkimage.py --selftest

Was dabei entsteht, ist eine Datei, die genau so in den Flash geschrieben wird:
ein Kopf von 4 KiB und dahinter ein FAT16-Dateisystem mit den drei portablen
Programmen. **Hinein kommt das nur ueber ein signiertes Firmware-Update** -
nie ueber das Laufwerk selbst, das weist jeden Schreibbefehl ab
(apps/firmware/src/classes/transport/usb_msc.c).

Der Kopf traegt den SHA-256 des Dateisystems. Die Firmware kann damit vor dem
Anbieten pruefen, ob im Flash wirklich das steht, was eingespielt wurde; ein
halb geschriebenes Abbild wird dann gar nicht erst als Laufwerk gezeigt.

Das Abbild ist **reproduzierbar**: gleiche Programme, gleiche Bytes. Darum
feste Zeitstempel und eine Datentraegernummer, die aus dem Inhalt kommt - sonst
sieht jedes Bauen nach einer Aenderung aus und niemand kann zwei Abbilder
vergleichen. Siehe docs/laufwerk.md. Nur Standardbibliothek.
"""
import argparse
import hashlib
import io
import os
import struct
import sys

SECTOR = 512
HEADER_LEN = 4096
# Kleinste loeschbare Einheit des W25Q512JV. Die Partition belegt ganze
# Sektoren, sonst teilt sie sich einen mit dem Nachbarn - und ein Loeschen
# wuerde dessen Daten mitnehmen.
FLASH_SECTOR = 4096
HEADER_MAGIC = b'OSKDRIVE'
HEADER_VERSION = 1

# Feste Zeit in jedem Verzeichniseintrag: 01.01.2026, 00:00:00. FAT kann nichts
# vor 1980, und eine echte Uhrzeit wuerde die Reproduzierbarkeit kosten.
FAT_DATE = ((2026 - 1980) << 9) | (1 << 5) | 1
FAT_TIME = 0

VOLUME_LABEL = b'OSK        '
OEM_NAME = b'SORGLOS '

# Verzeichniseintraege: schreibgeschuetzt (0x01) und Archiv (0x20). Das Bit
# ist Hoeflichkeit gegenueber dem Dateimanager; der echte Schutz sitzt im
# Geraet, das jeden Schreibbefehl abweist.
ATTR_FILE = 0x21
ATTR_VOLUME = 0x08

# Mindestzahl Zuordnungseinheiten eines FAT16-Datentraegers. Darunter liest
# ein Betriebssystem das Dateisystem als FAT12.
FAT16_MIN_CLUSTERS = 4085
FAT16_MAX_CLUSTERS = 65524

LIESMICH = u"""openSecureKey - die Verwaltungsprogramme

Dieses Laufwerk gehoert zum Stick und ist schreibgeschuetzt. Programm starten,
fertig: keine Installation, keine Administratorrechte.

  OSK-WIN.EXE   Windows
  OSK-LIN       Linux
  OSK-MAC       macOS

Nichts auf diesem Laufwerk laesst sich aendern. Neue Fassungen kommen ueber ein
signiertes Firmware-Update, nicht ueber dieses Laufwerk.

-- English --------------------------------------------------------------
This drive belongs to the key and is read-only. Run the program for your
system: no installation, no administrator rights. Nothing here can be
changed; new versions arrive through a signed firmware update.

-- Francais -------------------------------------------------------------
Ce lecteur appartient a la cle et est en lecture seule. Lancez le programme
de votre systeme : aucune installation, aucun droit administrateur. Rien ne
peut etre modifie ici ; les nouvelles versions arrivent par une mise a jour
signee du micrologiciel.

-- Italiano -------------------------------------------------------------
Questa unita appartiene alla chiave ed e di sola lettura. Avviare il
programma del proprio sistema: nessuna installazione, nessun diritto di
amministratore. Qui non si puo modificare nulla; le nuove versioni arrivano
con un aggiornamento firmato del firmware.

-- Espanol --------------------------------------------------------------
Esta unidad pertenece a la llave y es de solo lectura. Ejecute el programa
de su sistema: sin instalacion, sin permisos de administrador. Aqui no se
puede cambiar nada; las versiones nuevas llegan con una actualizacion
firmada del firmware.
"""


class ImageError(Exception):
    """Etwas stimmt mit den Eingaben oder dem Abbild nicht."""


def short_name(name):
    """Wandelt einen Namen in die 8.3-Form, wie FAT sie im Verzeichnis fuehrt.

    Lange Namen (VFAT) sind bewusst nicht dabei: sie waeren ein zweiter,
    laengerer Parser auf einer Flaeche, die jeder Rechner liest. Vier kurze
    Namen tun es auch.
    """
    base, _, ext = name.partition('.')
    if not base or len(base) > 8 or len(ext) > 3:
        raise ImageError('Name passt nicht in 8.3: %r' % name)
    allowed = set('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_')
    for char in (base + ext).upper():
        if char not in allowed:
            raise ImageError('Zeichen %r geht in einem 8.3-Namen nicht' % char)
    return (base.upper().ljust(8) + ext.upper().ljust(3)).encode('ascii')


def pick_cluster_sectors(total_bytes):
    """Kleinste Zuordnungseinheit, mit der das alles noch FAT16 ist.

    Klein heisst wenig Verschnitt - und der Stick hat keinen Platz zu
    verschenken. Groesser wird nur, was sonst ueber die Clustergrenze von
    FAT16 liefe.
    """
    for sectors in (1, 2, 4, 8, 16, 32, 64):
        size = sectors * SECTOR
        clusters = (total_bytes + size - 1) // size
        if max(clusters, FAT16_MIN_CLUSTERS) <= FAT16_MAX_CLUSTERS:
            return sectors
    raise ImageError('%d Byte passen in kein FAT16' % total_bytes)


def build_fat16(files, root_entries=512):
    """Baut das Dateisystem. files ist eine Liste (8.3-Name, Bytes).

    Liefert (abbild, zahlen) - die Zahlen gehen in die Doku und in die
    Ausgabe, damit der Platzbedarf nachgerechnet und nicht geglaubt wird.
    """
    if not files:
        raise ImageError('ohne Dateien kein Laufwerk')
    if len(files) + 1 > root_entries:
        raise ImageError('zu viele Dateien fuer das Stammverzeichnis')

    payload = sum(len(data) for _, data in files)
    spc = pick_cluster_sectors(payload)
    cluster_bytes = spc * SECTOR

    # Jede Datei belegt ganze Zuordnungseinheiten. Eine leere Datei belegt
    # keine - sie hat keine erste Einheit.
    used = sum((len(data) + cluster_bytes - 1) // cluster_bytes for _, data in files)
    clusters = max(used, FAT16_MIN_CLUSTERS)
    # Zwei Plaetze der Tabelle sind reserviert und gehoeren keiner Datei.
    fat_entries = clusters + 2
    fat_sectors = (fat_entries * 2 + SECTOR - 1) // SECTOR
    root_sectors = (root_entries * 32 + SECTOR - 1) // SECTOR
    reserved = 1
    data_sectors = clusters * spc
    total_sectors = reserved + 2 * fat_sectors + root_sectors + data_sectors

    if total_sectors > 0xFFFFFFFF:
        raise ImageError('Datentraeger zu gross')

    # --- Dateizuordnungstabelle und Datenbereich --------------------------
    fat = [0] * fat_entries
    fat[0] = 0xFFF8
    fat[1] = 0xFFFF
    data = bytearray(data_sectors * SECTOR)
    entries = bytearray()
    entries += VOLUME_LABEL + struct.pack('<BBBHHHHHHHI', ATTR_VOLUME, 0, 0, FAT_TIME,
                                          FAT_DATE, FAT_DATE, 0, FAT_TIME, FAT_DATE, 0, 0)

    next_cluster = 2
    for name, blob in files:
        count = (len(blob) + cluster_bytes - 1) // cluster_bytes
        first = next_cluster if count else 0
        for index in range(count):
            here = next_cluster + index
            fat[here] = 0xFFFF if index + 1 == count else here + 1
            chunk = blob[index * cluster_bytes:(index + 1) * cluster_bytes]
            at = (here - 2) * cluster_bytes
            data[at:at + len(chunk)] = chunk
        next_cluster += count
        entries += short_name(name) + struct.pack(
            '<BBBHHHHHHHI', ATTR_FILE, 0, 0, FAT_TIME, FAT_DATE, FAT_DATE, 0,
            FAT_TIME, FAT_DATE, first, len(blob))

    root = bytes(entries).ljust(root_sectors * SECTOR, b'\x00')
    fat_bytes = b''.join(struct.pack('<H', value) for value in fat)
    fat_bytes = fat_bytes.ljust(fat_sectors * SECTOR, b'\x00')

    # --- Startsektor ------------------------------------------------------
    # Die Datentraegernummer kommt aus dem Inhalt: gleiche Programme, gleiche
    # Nummer, gleiches Abbild.
    volume_id = struct.unpack('<I', hashlib.sha256(
        b''.join(short_name(n) + d for n, d in files)).digest()[:4])[0]

    boot = bytearray(SECTOR)
    boot[0:3] = b'\xeb\x3c\x90'
    boot[3:11] = OEM_NAME
    struct.pack_into('<HBHBHHBHHHII', boot, 11,
                     SECTOR,              # Byte je Sektor
                     spc,                 # Sektoren je Zuordnungseinheit
                     reserved,            # reservierte Sektoren
                     2,                   # Zahl der Tabellen
                     root_entries,        # Eintraege im Stammverzeichnis
                     total_sectors if total_sectors <= 0xFFFF else 0,
                     0xF8,                # Medienkennung
                     fat_sectors,         # Sektoren je Tabelle
                     63, 255, 0,          # Geometrie, verborgene Sektoren
                     0 if total_sectors <= 0xFFFF else total_sectors)
    boot[36] = 0x80          # Laufwerksnummer
    boot[37] = 0
    boot[38] = 0x29          # erweiterte Kennung folgt
    struct.pack_into('<I', boot, 39, volume_id)
    boot[43:54] = VOLUME_LABEL
    boot[54:62] = b'FAT16   '
    boot[510] = 0x55
    boot[511] = 0xAA

    image = bytes(boot) + fat_bytes + fat_bytes + root + bytes(data)
    if len(image) != total_sectors * SECTOR:
        raise ImageError('Abbild hat %d statt %d Byte' % (len(image), total_sectors * SECTOR))

    numbers = {
        'files': [(name, len(blob)) for name, blob in files],
        'payload': payload,
        'cluster_bytes': cluster_bytes,
        'clusters': clusters,
        'fat_sectors': fat_sectors,
        'root_sectors': root_sectors,
        'overhead': len(image) - payload,
        'image': len(image),
        'blocks': total_sectors,
    }
    return image, numbers


def wrap(image):
    """Legt den Kopf davor und fuellt auf ganze Flash-Sektoren auf.

    Aufgefuellt wird mit 0xFF, dem geloeschten Zustand des Bausteins: so ist
    das Einspielen ein geradliniges Schreiben ganzer Sektoren und kein
    Lesen-Aendern-Schreiben am Rand. Die Pruefsumme deckt nur das
    Dateisystem ab, dessen Laenge im Kopf steht - die Fuellbytes stoeren sie
    darum nicht.
    """
    head = bytearray(b'\xff' * HEADER_LEN)
    head[0:8] = HEADER_MAGIC
    struct.pack_into('<IIII', head, 8, HEADER_VERSION, len(image), SECTOR,
                     len(image) // SECTOR)
    head[24:56] = hashlib.sha256(image).digest()
    blob = bytes(head) + image
    pad = (-len(blob)) % FLASH_SECTOR
    return blob + b'\xff' * pad


def flash_bytes(image_len):
    """Wie viel Flash die Laufwerkspartition wirklich belegt."""
    total = HEADER_LEN + image_len
    return total + ((-total) % FLASH_SECTOR)


def unwrap(blob):
    """Liest den Kopf und gibt das Dateisystem zurueck. Prueft alles, bevor es
    etwas glaubt - diese Datei kann aus einem halb beschriebenen Flash kommen."""
    if len(blob) < HEADER_LEN:
        raise ImageError('kuerzer als der Kopf')
    if blob[0:8] != HEADER_MAGIC:
        raise ImageError('kein Laufwerksabbild')
    version, length, sector, blocks = struct.unpack_from('<IIII', blob, 8)
    if version != HEADER_VERSION:
        raise ImageError('Fassung %d kennt dieses Werkzeug nicht' % version)
    if sector != SECTOR:
        raise ImageError('Blockgroesse %d statt %d' % (sector, SECTOR))
    if length % SECTOR or blocks * SECTOR != length:
        raise ImageError('Laenge %d passt nicht zu %d Bloecken' % (length, blocks))
    if len(blob) < HEADER_LEN + length:
        raise ImageError('Abbild ist abgeschnitten: %d von %d Byte'
                         % (len(blob) - HEADER_LEN, length))
    image = blob[HEADER_LEN:HEADER_LEN + length]
    if hashlib.sha256(image).digest() != blob[24:56]:
        raise ImageError('Pruefsumme stimmt nicht')
    return image


def read_fat16(image):
    """Liest das Dateisystem wieder aus - unabhaengig vom Bauen.

    Das ist die Gegenprobe: dieselben Zahlen zweimal zu berechnen beweist
    nichts, aber ein Abbild, aus dem die Dateien wieder herauskommen, schon.
    """
    if len(image) < SECTOR:
        raise ImageError('zu kurz fuer einen Startsektor')
    if image[510] != 0x55 or image[511] != 0xAA:
        raise ImageError('Startsektor ohne Kennung')
    (bps, spc, reserved, fats, root_entries, small_total, _media, fat_sectors) = \
        struct.unpack_from('<HBHBHHBH', image, 11)
    big_total = struct.unpack_from('<I', image, 32)[0]
    total = small_total or big_total
    if bps != SECTOR or spc == 0 or fats == 0 or fat_sectors == 0:
        raise ImageError('Startsektor unbrauchbar')
    if total * SECTOR != len(image):
        raise ImageError('Startsektor nennt %d Sektoren, die Datei hat %d'
                         % (total, len(image) // SECTOR))

    fat_at = reserved * SECTOR
    root_at = fat_at + fats * fat_sectors * SECTOR
    root_sectors = (root_entries * 32 + SECTOR - 1) // SECTOR
    data_at = root_at + root_sectors * SECTOR
    cluster_bytes = spc * SECTOR
    clusters = (len(image) - data_at) // cluster_bytes

    def fat_entry(index):
        if index * 2 + 2 > fat_sectors * SECTOR:
            raise ImageError('Tabellenplatz %d liegt ausserhalb' % index)
        return struct.unpack_from('<H', image, fat_at + index * 2)[0]

    # Beide Tabellen muessen gleich sein, sonst ist das Abbild fragwuerdig.
    first = image[fat_at:fat_at + fat_sectors * SECTOR]
    second = image[root_at - fat_sectors * SECTOR:root_at]
    if first != second:
        raise ImageError('die beiden Tabellen sind verschieden')

    out = []
    for index in range(root_entries):
        at = root_at + index * 32
        entry = image[at:at + 32]
        if entry[0] in (0x00, 0xE5):
            continue
        attr = entry[11]
        if attr & ATTR_VOLUME:
            continue
        name = entry[0:8].decode('ascii').rstrip()
        ext = entry[8:11].decode('ascii').rstrip()
        start, size = struct.unpack_from('<HI', entry, 26)
        blob = bytearray()
        chain = start
        seen = 0
        while size and chain >= 2:
            if chain - 2 >= clusters:
                raise ImageError('Einheit %d liegt ausserhalb' % chain)
            at = data_at + (chain - 2) * cluster_bytes
            blob += image[at:at + cluster_bytes]
            seen += 1
            if seen > clusters:
                raise ImageError('die Kette von %s laeuft im Kreis' % name)
            nxt = fat_entry(chain)
            if nxt >= 0xFFF8:
                break
            chain = nxt
        if len(blob) < size:
            raise ImageError('%s ist kuerzer als angekuendigt' % name)
        out.append((name + ('.' + ext if ext else ''), bytes(blob[:size])))
    return out


def human(value):
    return '%.2f MB (%d B)' % (value / 1000000.0, value)


def report(numbers, out=sys.stdout):
    out.write('Abbild des Laufwerks\n')
    for name, size in numbers['files']:
        out.write('  %-14s %s\n' % (name, human(size)))
    out.write('  %-14s %s\n' % ('Nutzdaten', human(numbers['payload'])))
    out.write('  %-14s %d B je Zuordnungseinheit, %d Einheiten\n'
              % ('FAT16', numbers['cluster_bytes'], numbers['clusters']))
    out.write('  %-14s %s (Startsektor, 2 Tabellen zu %d Sektoren, '
              'Stammverzeichnis %d Sektoren, Verschnitt)\n'
              % ('Verwaltung', human(numbers['overhead']), numbers['fat_sectors'],
                 numbers['root_sectors']))
    out.write('  %-14s %s in %d Bloecken\n'
              % ('Dateisystem', human(numbers['image']), numbers['blocks']))
    total = flash_bytes(numbers['image'])
    out.write('  %-14s %s (mit Kopf von %d B, auf %d-B-Sektoren aufgefuellt)\n'
              % ('im Flash', human(total), HEADER_LEN, FLASH_SECTOR))


def read_file(path):
    with open(path, 'rb') as handle:
        return handle.read()


def build_from_paths(windows, linux, macos):
    files = [
        ('OSK-WIN.EXE', read_file(windows)),
        ('OSK-LIN', read_file(linux)),
        ('OSK-MAC', read_file(macos)),
        ('LIESMICH.TXT', LIESMICH.replace(u'\n', u'\r\n').encode('utf-8')),
    ]
    return build_fat16(files)


def selftest():
    """Baut ein Abbild, liest es unabhaengig wieder aus und prueft den Kopf.

    Das Werkzeug prueft damit zuerst sich selbst: ein Abbild, aus dem die
    Dateien nicht byteweise zurueckkommen, ist keines - auch wenn alle Zahlen
    schoen aussehen.
    """
    fails = []
    checks = [0]

    def check(what, condition):
        checks[0] += 1
        if not condition:
            fails.append(what)

    # Drei Blobs, die sich unterscheiden lassen, einer davon ueber mehrere
    # Zuordnungseinheiten hinweg und einer mit krummer Laenge.
    blobs = [
        ('OSK-WIN.EXE', bytes(bytearray((i * 7 + 1) & 0xFF for i in range(70001)))),
        ('OSK-LIN', bytes(bytearray((i * 13 + 5) & 0xFF for i in range(512)))),
        ('OSK-MAC', bytes(bytearray((i * 29 + 9) & 0xFF for i in range(1)))),
        ('LIESMICH.TXT', LIESMICH.replace(u'\n', u'\r\n').encode('utf-8')),
    ]
    image, numbers = build_fat16(blobs)

    check('Abbild ist ein Vielfaches der Blockgroesse', len(image) % SECTOR == 0)
    check('Blockzahl passt', numbers['blocks'] * SECTOR == len(image))
    check('Nutzdaten stimmen', numbers['payload'] == sum(len(b) for _, b in blobs))

    back = dict(read_fat16(image))
    check('alle Dateien wieder da', len(back) == len(blobs))
    for name, blob in blobs:
        check('%s kommt byteweise zurueck' % name, back.get(name) == blob)

    # Reproduzierbar: zweimal bauen, zweimal dieselben Bytes.
    again, _ = build_fat16(blobs)
    check('zweimal gebaut ist zweimal gleich', again == image)

    # Kopf und Pruefsumme.
    wrapped = wrap(image)
    check('ganze Flash-Sektoren', len(wrapped) % FLASH_SECTOR == 0)
    check('Laenge wie gerechnet', len(wrapped) == flash_bytes(len(image)))
    check('nicht mehr als ein Sektor Fuellung',
          len(wrapped) - HEADER_LEN - len(image) < FLASH_SECTOR)
    check('Kopf laesst sich wieder abnehmen', unwrap(wrapped) == image)

    broken = bytearray(wrapped)
    broken[HEADER_LEN + 600] ^= 0x01
    try:
        unwrap(bytes(broken))
        check('ein verdrehtes Bit faellt auf', False)
    except ImageError:
        check('ein verdrehtes Bit faellt auf', True)

    try:
        unwrap(wrapped[:HEADER_LEN + len(image) - SECTOR])
        check('ein abgeschnittenes Abbild faellt auf', False)
    except ImageError:
        check('ein abgeschnittenes Abbild faellt auf', True)

    # Ein Name, der nicht in 8.3 passt, wird abgelehnt statt abgeschnitten.
    for bad in ('VIELZULANGERNAME.EXE', 'OSK.EXEC', 'OSK WIN.EXE', ''):
        try:
            short_name(bad)
            check('Name %r abgelehnt' % bad, False)
        except ImageError:
            check('Name %r abgelehnt' % bad, True)

    # Die Grenze von FAT16 wird eingehalten, nicht gestreift.
    for size in (1, 10 ** 6, 28 * 10 ** 6, 64 * 10 ** 6):
        spc = pick_cluster_sectors(size)
        count = (size + spc * SECTOR - 1) // (spc * SECTOR)
        check('%d B bleibt unter der FAT16-Grenze' % size, count <= FAT16_MAX_CLUSTERS)

    report(numbers)
    print('selbsttest: %d Faelle, %d Fehler' % (checks[0], len(fails)))
    for item in fails:
        print('  FEHLER: %s' % item)
    if not fails:
        print('pruefung: ok')
    return 0 if not fails else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description='Abbild des Laufwerks bauen')
    parser.add_argument('--windows')
    parser.add_argument('--linux')
    parser.add_argument('--macos')
    parser.add_argument('--out')
    parser.add_argument('--check', help='ein fertiges Abbild pruefen')
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args(argv)

    if args.selftest:
        return selftest()

    if args.check:
        try:
            image = unwrap(read_file(args.check))
            files = read_fat16(image)
        except ImageError as error:
            print('FEHLER: %s' % error)
            return 1
        print('%s: in Ordnung, %d Dateien' % (args.check, len(files)))
        for name, blob in files:
            print('  %-14s %s' % (name, human(len(blob))))
        return 0

    missing = [flag for flag, value in (('--windows', args.windows), ('--linux', args.linux),
                                        ('--macos', args.macos), ('--out', args.out))
               if not value]
    if missing:
        print('FEHLER: es fehlt %s' % ', '.join(missing))
        return 2

    try:
        image, numbers = build_from_paths(args.windows, args.linux, args.macos)
    except (ImageError, IOError, OSError) as error:
        print('FEHLER: %s' % error)
        return 1

    # Gegenprobe vor dem Schreiben: was nicht wieder herauskommt, wird nicht
    # ausgeliefert.
    back = dict(read_fat16(image))
    for name, size in numbers['files']:
        if name not in back or len(back[name]) != size:
            print('FEHLER: %s kommt aus dem eigenen Abbild nicht zurueck' % name)
            return 1

    folder = os.path.dirname(os.path.abspath(args.out))
    if folder and not os.path.isdir(folder):
        os.makedirs(folder)
    with open(args.out, 'wb') as handle:
        handle.write(wrap(image))

    report(numbers)
    print('geschrieben: %s' % args.out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
