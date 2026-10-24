# -*- coding: utf-8 -*-
"""ed25519 nach RFC 8032, nur Standardbibliothek.

Geschrieben aus RFC 8032 Abschnitt 5.1 - kein Fremdcode, nichts kopiert.
Geprueft gegen die offiziellen Testvektoren des RFC (tools/tests/test_sign.py).

WOFUER DAS DA IST und wofuer nicht:

- Hier wird UNTERSCHRIEBEN, auf dem Rechner des Entwicklers, mit einem
  Schluessel, der ausserhalb des Projektordners liegt. Das ist eine
  Werkzeugaufgabe.
- Im GERAET prueft MCUboot, und MCUboot bringt seine eigene, gepruefte
  ed25519-Umsetzung aus dem SDK mit. Dieser Code laeuft nie auf dem Geraet.
- Ist das Paket `cryptography` installiert, wird es benutzt und das Ergebnis
  gegen diese Umsetzung verglichen (sign.py --selftest). Zwei unabhaengige
  Wege, die dasselbe liefern muessen.

Die Umsetzung ist auf Lesbarkeit gebaut, nicht auf Geschwindigkeit, und sie
ist NICHT gegen Zeitmessung gehaertet. Fuer einen Schluessel, der auf dem
eigenen Rechner in einer Datei liegt, ist das die richtige Abwaegung; fuer
einen Dienst im Netz waere es die falsche.
"""
import hashlib

# Kurvenparameter, RFC 8032 Abschnitt 5.1
P = 2 ** 255 - 19
L = 2 ** 252 + 27742317777372353535851937790883648493
D = (-121665 * pow(121666, P - 2, P)) % P
BASE_Y = (4 * pow(5, P - 2, P)) % P
BITS = 256


def _sha512(data):
    return hashlib.sha512(data).digest()


def _recover_x(y, sign):
    """x aus y zurueckrechnen (RFC 8032, 5.1.3)."""
    if y >= P:
        return None
    xx = (y * y - 1) * pow(D * y * y + 1, P - 2, P)
    if xx % P == 0:
        return 0 if sign == 0 else None
    x = pow(xx, (P + 3) // 8, P)
    if (x * x - xx) % P != 0:
        x = (x * pow(2, (P - 1) // 4, P)) % P
    if (x * x - xx) % P != 0:
        return None
    if x % 2 != sign:
        x = P - x
    return x


# Punkte in erweiterten Koordinaten (x, y, z, t) - so bleibt jede Addition
# ohne Division, und Division ist in einem Restklassenring teuer.
BASE = (_recover_x(BASE_Y, 0), BASE_Y, 1, (_recover_x(BASE_Y, 0) * BASE_Y) % P)
ZERO = (0, 1, 1, 0)


def _add(a, b):
    x1, y1, z1, t1 = a
    x2, y2, z2, t2 = b
    aa = (y1 - x1) * (y2 - x2) % P
    bb = (y1 + x1) * (y2 + x2) % P
    cc = 2 * t1 * t2 * D % P
    dd = 2 * z1 * z2 % P
    e, f, g, h = bb - aa, dd - cc, dd + cc, bb + aa
    return (e * f % P, g * h % P, f * g % P, e * h % P)


def _mul(point, scalar):
    result = ZERO
    while scalar > 0:
        if scalar & 1:
            result = _add(result, point)
        point = _add(point, point)
        scalar >>= 1
    return result


def _equal(a, b):
    x1, y1, z1, _ = a
    x2, y2, z2, _ = b
    return (x1 * z2 - x2 * z1) % P == 0 and (y1 * z2 - y2 * z1) % P == 0


def _encode(point):
    x, y, z, _ = point
    inv = pow(z, P - 2, P)
    x, y = x * inv % P, y * inv % P
    return int.to_bytes(y | ((x & 1) << 255), 32, 'little')


def _decode(data):
    if len(data) != 32:
        return None
    number = int.from_bytes(data, 'little')
    sign = number >> 255
    y = number & ((1 << 255) - 1)
    x = _recover_x(y, sign)
    if x is None:
        return None
    return (x, y, 1, x * y % P)


def _clamp(h):
    """Die ersten 32 Byte des Hashes zum Skalar machen (RFC 8032, 5.1.5)."""
    a = bytearray(h[:32])
    a[0] &= 248
    a[31] &= 127
    a[31] |= 64
    return int.from_bytes(bytes(a), 'little')


def public_key(seed):
    """Oeffentlicher Teil (32 Byte) zum Samen (32 Byte)."""
    if len(seed) != 32:
        raise ValueError('Samen muss 32 Byte lang sein, nicht %d' % len(seed))
    return _encode(_mul(BASE, _clamp(_sha512(seed))))


def sign(seed, message):
    """Signatur (64 Byte) ueber `message` mit dem Samen `seed`."""
    if len(seed) != 32:
        raise ValueError('Samen muss 32 Byte lang sein, nicht %d' % len(seed))
    h = _sha512(seed)
    a = _clamp(h)
    prefix = h[32:]
    pub = _encode(_mul(BASE, a))
    r = int.from_bytes(_sha512(prefix + message), 'little') % L
    big_r = _encode(_mul(BASE, r))
    k = int.from_bytes(_sha512(big_r + pub + message), 'little') % L
    s = (r + k * a) % L
    return big_r + int.to_bytes(s, 32, 'little')


def verify(pub, message, signature):
    """True, wenn die Signatur zum oeffentlichen Teil und zur Nachricht passt."""
    if len(pub) != 32 or len(signature) != 64:
        return False
    point_a = _decode(pub)
    point_r = _decode(signature[:32])
    if point_a is None or point_r is None:
        return False
    s = int.from_bytes(signature[32:], 'little')
    if s >= L:
        # Eine zu grosse Zahl waere eine zweite gueltige Form derselben
        # Signatur. RFC 8032 verlangt die Ablehnung.
        return False
    k = int.from_bytes(_sha512(signature[:32] + pub + message), 'little') % L
    left = _mul(BASE, s)
    right = _add(point_r, _mul(point_a, k))
    return _equal(left, right)
