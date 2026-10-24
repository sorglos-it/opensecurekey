"""Ed25519: NUR pruefen, nie unterschreiben.

Python hat kein Ed25519 in der Standardbibliothek, und Fremdcode ist hier
nicht erlaubt. Darum sind die Formeln aus RFC 8032 (Abschnitt 5.1.7,
"Verify") hier mit Pythons eigenen ganzen Zahlen ausgeschrieben.

Absichtlich fehlt das Unterschreiben: Pruefen beruehrt kein Geheimnis, es
gibt also keinen Seitenkanal zu verlieren, und ein falsches Ergebnis ist
eine gescheiterte Pruefung und kein verratener Schluessel. Unterschrieben
wird im Chip.

Streng gepruefte Punkte, damit das Urteil dasselbe ist wie in Go
(crypto/ed25519) und in C:

* S muss kleiner als die Gruppenordnung L sein. Eine Unterschrift mit
  S >= L wird abgewiesen, auch wenn sie "rechnerisch" aufgehen wuerde.
* Der oeffentliche Schluessel muss ein Punkt auf der Kurve in kanonischer
  Schreibweise sein (y < p).
* Geprueft wird [S]B - [k]A und das Ergebnis BYTEWEISE gegen R aus der
  Unterschrift. Damit ist auch ein nicht kanonisches R abgewiesen, ohne
  dass es eine eigene Regel braucht.

Nur Standardbibliothek: hashlib fuer SHA-512 und SHA-256.
"""

import hashlib

# Kurvenparameter von edwards25519 nach RFC 8032, Abschnitt 5.1.
P = 2**255 - 19
# d = -121665 / 121666 (mod p)
D = (-121665 * pow(121666, P - 2, P)) % P
# L ist die Ordnung der Hauptuntergruppe.
L = 2**252 + 27742317777372353535851937790883648493
# Quadratwurzel aus -1 (mod p), gebraucht beim Zurueckrechnen von x.
SQRT_M1 = pow(2, (P - 1) // 4, P)

# Der Basispunkt B.
B_X = 15112221349535400772501151409588531511454012693041857206046113283949847762202
B_Y = 46316835694926478169428394003475163141307993866256225615783033603165251855960

KEY_LEN = 32
SIG_LEN = 64

# Punkte liegen in erweiterten Koordinaten (X : Y : Z : T) mit x = X/Z,
# y = Y/Z und T = X*Y/Z. Das spart die Division bei jeder Verdopplung.
_NEUTRAL = (0, 1, 1, 0)


def _point_add(first, second):
    """Addiert zwei Punkte in erweiterten Koordinaten."""
    x1, y1, z1, t1 = first
    x2, y2, z2, t2 = second
    a = ((y1 - x1) * (y2 - x2)) % P
    b = ((y1 + x1) * (y2 + x2)) % P
    c = (2 * t1 * t2 * D) % P
    e = (2 * z1 * z2) % P
    f = (b - a) % P
    g = (e - c) % P
    h = (e + c) % P
    i = (b + a) % P
    return (f * g % P, h * i % P, g * h % P, f * i % P)


def _point_double(point):
    """Verdoppelt einen Punkt."""
    return _point_add(point, point)


def _point_mul(scalar, point):
    """Rechnet scalar * point. Die Laufzeit haengt vom Skalar ab - das ist
    hier erlaubt, weil beide Werte oeffentlich sind: eine Unterschrift und
    ein oeffentlicher Schluessel. Es gibt kein Geheimnis zu verraten."""
    result = _NEUTRAL
    while scalar > 0:
        if scalar & 1:
            result = _point_add(result, point)
        point = _point_double(point)
        scalar >>= 1
    return result


def _point_negate(point):
    """Spiegelt einen Punkt: -(x, y) ist (-x, y)."""
    x, y, z, t = point
    return ((-x) % P, y, z, (-t) % P)


def _point_encode(point):
    """Schreibt einen Punkt als 32 Byte: y klein zuerst, das oberste Bit
    traegt das Vorzeichen von x."""
    x, y, z, _ = point
    inverse = pow(z, P - 2, P)
    affine_x = x * inverse % P
    affine_y = y * inverse % P
    raw = bytearray(affine_y.to_bytes(KEY_LEN, "little"))
    raw[31] |= (affine_x & 1) << 7
    return bytes(raw)


def _recover_x(y, sign):
    """Rechnet x aus y zurueck. Gibt None, wenn es kein solches x gibt -
    dann liegt der Punkt nicht auf der Kurve."""
    if y >= P:
        # Nicht kanonisch geschrieben. Go weist das ebenfalls ab.
        return None
    y2 = y * y % P
    numerator = (y2 - 1) % P
    denominator = (D * y2 + 1) % P
    if denominator == 0:
        return None
    square = numerator * pow(denominator, P - 2, P) % P
    if square == 0:
        # x ist 0; ein gesetztes Vorzeichenbit waere dann nicht kanonisch.
        return None if sign else 0
    x = pow(square, (P + 3) // 8, P)
    if x * x % P != square:
        x = x * SQRT_M1 % P
    if x * x % P != square:
        return None
    if x & 1 != sign:
        x = (P - x) % P
    return x


def _point_decode(raw):
    """Liest einen Punkt aus 32 Byte. Gibt None bei allem, was kein
    kanonischer Punkt auf der Kurve ist."""
    if len(raw) != KEY_LEN:
        return None
    value = int.from_bytes(raw, "little")
    sign = (value >> 255) & 1
    y = value & ((1 << 255) - 1)
    x = _recover_x(y, sign)
    if x is None:
        return None
    return (x, y, 1, x * y % P)


def verify(public_key, message, signature):
    """Prueft eine Ed25519-Unterschrift ueber die Nachricht selbst (nicht
    ueber einen Vorab-Hash). Rueckgabe True nur bei einer gueltigen
    Unterschrift; jeder Fehler ist ein sauberes False, keine Ausnahme."""
    if public_key is None or signature is None or message is None:
        return False
    public_key = bytes(public_key)
    signature = bytes(signature)
    message = bytes(message)
    if len(public_key) != KEY_LEN or len(signature) != SIG_LEN:
        return False

    point_a = _point_decode(public_key)
    if point_a is None:
        return False

    signature_r = signature[:32]
    scalar_s = int.from_bytes(signature[32:], "little")
    # Die Pflichtpruefung: S muss kleiner als die Gruppenordnung sein.
    if scalar_s >= L:
        return False

    digest = hashlib.sha512(signature_r + public_key + message).digest()
    scalar_k = int.from_bytes(digest, "little") % L

    base = (B_X, B_Y, 1, B_X * B_Y % P)
    # [S]B - [k]A muss genau R ergeben, Byte fuer Byte.
    checked = _point_add(_point_mul(scalar_s, base),
                         _point_mul(scalar_k, _point_negate(point_a)))
    return _point_encode(checked) == signature_r


def sha512(data):
    """SHA-512 aus der Standardbibliothek - hier nur, damit die Pruefwerte
    gegen dieselbe Stelle laufen wie die Unterschriftenpruefung."""
    return hashlib.sha512(bytes(data)).digest()


def sha256(data):
    """SHA-256 aus der Standardbibliothek."""
    return hashlib.sha256(bytes(data)).digest()
