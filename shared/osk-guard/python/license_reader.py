"""Lizenzen: Aufbau pruefen, Unterschrift pruefen, und ERST DANN die Felder
aus der geprueften Nutzlast lesen.

Das ist der Punkt, an dem diese Bibliothek dem lokalen Dienst nichts
glaubt. Frueher gab der Dienst die fertig gelesenen Felder heraus - dann
haette eine falsche Attrappe auf der Rueckschleife Lizenzen erfinden
koennen, ohne das geschuetzte Programm anzufassen. Das ist eine niedrigere
Huerde als ein Disassembler, also ist es jetzt anders: der Dienst gibt den
unterschriebenen Block heraus, und geprueft wird hier.

Dazu steckt hier ein CBOR-Leser fuer genau eine Aufgabe: die Nutzlast einer
Lizenz. Es ist nicht der ganze Standard und soll es nicht sein. Gelesen
werden vorzeichenlose Zahlen, Texte, Felder und Abbildungen. Alles andere
wird abgewiesen, statt geraten - eine Nutzlast, die mehr enthaelt, kommt
nicht von unserem Unterschreiber.

Jede Laenge wird gegen das Pufferende geprueft, BEVOR gelesen wird. Diese
Bytes sind unterschrieben, aber der Leser muss auch mit unsinnigen
zurechtkommen: er laeuft, bevor die Unterschrift stimmt, wenn jemand ihn
falsch aufruft.

Nur Standardbibliothek, kein Fremdcode.
"""

import time

import ed25519_verify

# Aufbau einer Lizenz:
# "OSKLIC1" | Fassung(1) | Laenge der Nutzlast(2, gross zuerst) |
# Nutzlast | Ed25519-Unterschrift(64)
LICENSE_MAGIC = b"OSKLIC1"
LICENSE_VERSION = 1
LICENSE_HEAD_LEN = len(LICENSE_MAGIC) + 1 + 2
LICENSE_MAX_LOAD = 1024

KEY_LEN = 32
SIG_LEN = 64

# Die Tiefe, die diese Nutzlast braucht: Abbildung, darin ein Feld. Mehr
# ist ein Fehler.
CBOR_DEPTH = 4


class LicenseError(Exception):
    """Fehler beim Lesen einer Lizenz. code ist derselbe Schluessel wie in
    lang/<sprache>.json. osk_guard faengt ihn und gibt ihn als
    OskGuardError weiter, damit der Aufrufer nur eine Sorte Ausnahme
    kennen muss."""

    def __init__(self, code):
        super().__init__(code)
        self.code = code


def _fail(code):
    return LicenseError(code)


class License:
    """Eine gepruefte Lizenz. Jedes Feld steht so in der unterschriebenen
    Nutzlast - nichts davon ist eine Zusage des Dienstes."""

    def __init__(self, product="", holder="", number="", tiers=None,
                 seats=0, issued_utc=0, expires_utc=0):
        self.product = product
        self.holder = holder
        self.number = number
        self.tiers = list(tiers) if tiers else []
        self.seats = seats
        self.issued_utc = issued_utc
        self.expires_utc = expires_utc

    def expired(self):
        """Sagt, ob die Lizenz jetzt abgelaufen ist. 0 heisst: nie. Das
        ist eine Frage der Uhr, nicht der Unterschrift - eine abgelaufene
        Lizenz ist echt und vorbei. Gerechnet wird in UTC."""
        return self.expires_utc != 0 and int(time.time()) > self.expires_utc

    def has_tier(self, name):
        """Sagt, ob eine Ausbaustufe enthalten ist."""
        wanted = name.casefold()
        return any(have.casefold() == wanted for have in self.tiers)

    def __repr__(self):
        return ("License(product=%r, seats=%d, tiers=%r)" %
                (self.product, self.seats, self.tiers))


def license_verify(issuer_public_key, blob):
    """Prueft den Block gegen den Schluessel des Herstellers und gibt eine
    License zurueck. Die Reihenfolge ist Absicht: Aufbau, dann
    Unterschrift, dann erst die Felder."""
    if issuer_public_key is None or len(issuer_public_key) != KEY_LEN:
        raise _fail("error.guard.issuer")
    blob = bytes(blob) if blob else b""
    if len(blob) < LICENSE_HEAD_LEN + SIG_LEN:
        raise _fail("error.license.format")
    if blob[:len(LICENSE_MAGIC)] != LICENSE_MAGIC:
        raise _fail("error.license.format")
    if blob[len(LICENSE_MAGIC)] != LICENSE_VERSION:
        raise _fail("error.license.version")
    size = int.from_bytes(blob[len(LICENSE_MAGIC) + 1:LICENSE_HEAD_LEN], "big")
    # Die Gesamtlaenge muss genau stimmen: angehaengte Bytes werden nicht
    # stillschweigend uebergangen.
    if (size == 0 or size > LICENSE_MAX_LOAD or
            LICENSE_HEAD_LEN + size + SIG_LEN != len(blob)):
        raise _fail("error.license.format")
    load = blob[LICENSE_HEAD_LEN:LICENSE_HEAD_LEN + size]
    signature = blob[LICENSE_HEAD_LEN + size:]

    signed = LICENSE_MAGIC + b"\x00" + load
    if not ed25519_verify.verify(issuer_public_key, signed, signature):
        raise _fail("error.license.signature")
    # Erst ab hier sind die Bytes etwas wert.
    return license_fields(load)


def license_fields(load):
    """Liest die Felder aus der GEPRUEFTEN Nutzlast."""
    fields = cbor_map(load)
    found = License()

    product = fields.get("p")
    if not isinstance(product, str) or not product:
        raise _fail("error.license.product")
    found.product = product

    holder = fields.get("h")
    found.holder = holder if isinstance(holder, str) else ""
    number = fields.get("nr")
    found.number = number if isinstance(number, str) else ""

    seats = fields.get("s")
    if seats is not None:
        if not isinstance(seats, int) or seats > 0xffffffff:
            raise _fail("error.license.format")
        found.seats = seats

    for name, target in (("i", "issued_utc"), ("e", "expires_utc")):
        value = fields.get(name)
        if value is None:
            continue
        if not isinstance(value, int):
            raise _fail("error.license.format")
        if value > 1 << 62:
            raise _fail("error.license.time")
        setattr(found, target, value)

    listed = fields.get("t")
    if listed is not None:
        if not isinstance(listed, list):
            raise _fail("error.license.tier")
        for entry in listed:
            if not isinstance(entry, str) or not entry:
                raise _fail("error.license.tier")
            found.tiers.append(entry)
    return found


def cbor_map(data):
    """Liest eine Abbildung mit Textschluesseln aus den ganzen Bytes.
    Bleibt etwas uebrig, ist das ein Fehler."""
    value, rest = _cbor_value(memoryview(bytes(data)), 0)
    if len(rest) != 0:
        raise _fail("error.license.format")
    if not isinstance(value, dict):
        raise _fail("error.license.format")
    return value


def _cbor_value(data, depth):
    """Liest einen Wert und gibt (Wert, Rest) zurueck."""
    if depth > CBOR_DEPTH or len(data) == 0:
        raise _fail("error.license.format")
    major = data[0] >> 5
    count, rest = _cbor_head(data)
    if major == 0:
        # Vorzeichenlose Zahl.
        return count, rest
    if major == 3:
        # Text. Erst die Laenge gegen den Rest pruefen, dann lesen.
        if count > len(rest):
            raise _fail("error.license.format")
        raw = bytes(rest[:count])
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            raise _fail("error.license.format") from None
        return text, rest[count:]
    if major == 4:
        # Feld. Eine Anzahl groesser als der verbleibende Puffer kann nie
        # aufgehen - das ist immer ein Formatfehler, und so kann eine
        # erfundene Anzahl keinen Speicher anfordern.
        if count > len(rest):
            raise _fail("error.license.format")
        out = []
        for _ in range(count):
            entry, rest = _cbor_value(rest, depth + 1)
            out.append(entry)
        return out, rest
    if major == 5:
        # Abbildung. Je Eintrag zwei Werte, also mindestens zwei Byte -
        # eine groessere Anzahl kann nie aufgehen.
        if count > len(rest) // 2:
            raise _fail("error.license.format")
        out = {}
        for _ in range(count):
            key, rest = _cbor_value(rest, depth + 1)
            if not isinstance(key, str):
                raise _fail("error.license.format")
            if key in out:
                # Zweimal derselbe Schluessel: dann waere nicht
                # entschieden, welcher gilt.
                raise _fail("error.license.format")
            entry, rest = _cbor_value(rest, depth + 1)
            out[key] = entry
        return out, rest
    # Hauptsorten 1, 2, 6 und 7 braucht eine Lizenz nicht.
    raise _fail("error.license.format")


def _cbor_head(data):
    """Liest die Anzahl hinter der Hauptsorte und gibt (Anzahl, Rest).

    Je Laengenform wird die kuerzeste Schreibweise erzwungen: zwei
    Schreibweisen derselben Zahl darf es in unterschriebenen Daten nicht
    geben, sonst waere dieselbe Lizenz auf mehrere Weisen zu schreiben.

    Die vier- und achtbyteigen Formen braucht eine Lizenz wirklich: ein
    Zeitstempel in Sekunden ist rund 1,8 Milliarden und passt nicht in
    zwei Byte."""
    low = data[0] & 0x1f
    if low < 24:
        return low, data[1:]
    if low == 24:
        if len(data) < 2:
            raise _fail("error.license.format")
        if data[1] < 24:
            raise _fail("error.license.format")
        return data[1], data[2:]
    if low == 25:
        if len(data) < 3:
            raise _fail("error.license.format")
        value = int.from_bytes(data[1:3], "big")
        if value <= 0xff:
            raise _fail("error.license.format")
        return value, data[3:]
    if low == 26:
        if len(data) < 5:
            raise _fail("error.license.format")
        value = int.from_bytes(data[1:5], "big")
        if value <= 0xffff:
            raise _fail("error.license.format")
        return value, data[5:]
    if low == 27:
        if len(data) < 9:
            raise _fail("error.license.format")
        value = int.from_bytes(data[1:9], "big")
        if value <= 0xffffffff:
            raise _fail("error.license.format")
        return value, data[9:]
    # 28 bis 31 sind unbestimmte oder unzulaessige Formen.
    raise _fail("error.license.format")
