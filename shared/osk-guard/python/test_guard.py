"""Pruefsatz fuer die Python-Fassung von osk-guard.

    python -m unittest

Zwei Dinge werden geprueft:

1. Die eigene Ed25519-Pruefung und die Hashwerte gegen ../tests/vectors.json
   - dieselbe Datei, gegen die auch C, Go, PHP und .NET laufen. Jeder Fall
   muss genau so beurteilt werden wie dort vermerkt.
2. Der Zeilen-Leser des Drahtprotokolls: kaputte Zeile, ERR-Zeile,
   fehlendes Feld, falsche Laenge.

Die Fallzahl wird mitgeprueft. Null gelesene Faelle sind ein Fehler und
kein Erfolg: eine leere oder halbe Vektordatei darf nicht als gruener
Pruefsatz durchgehen.
"""

import json
import os
import socket
import threading
import unittest

import ed25519_verify
import license_reader
import osk_guard

# So viele Faelle muss die Vektordatei mindestens hergeben.
WANT_VERIFY = 21
WANT_VERIFY_VALID = 5
WANT_VERIFY_INVALID = 16
WANT_HASH = 9
# Fertig unterschriebene Lizenzen, erzeugt mit Go crypto/ed25519.
# 24 Faelle: 3 gueltige und 21 abzuweisende. Die drei Faelle
# zahl-in-zwei/vier/acht-byte-form sind nachgetragen worden, weil ohne sie
# die Mindestwertpruefung der langen Laengenformen ungemessen blieb - ein
# Leser ohne diese Pruefung nimmt dieselbe Zahl in vier Schreibweisen an.
WANT_LICENSE = 24
WANT_LICENSE_VALID = 3
WANT_LICENSE_INVALID = 21

TESTS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         os.pardir, "tests")
VECTORS_PATH = os.path.join(TESTS_DIR, "vectors.json")
LICENSE_VECTORS_PATH = os.path.join(TESTS_DIR, "license-vectors.json")


def load_vectors():
    """Liest die Vektordatei. Fehlt sie, scheitert der Pruefsatz - sie
    gehoert zur Pruefung und ist nicht wegzulassen."""
    with open(VECTORS_PATH, "r", encoding="utf-8") as handle:
        return json.load(handle)


def load_license_vectors():
    """Liest die Lizenzvektoren. Dieselbe Datei, gegen die auch die
    anderen Fassungen laufen."""
    with open(LICENSE_VECTORS_PATH, "r", encoding="utf-8") as handle:
        return json.load(handle)


class TestVectorFile(unittest.TestCase):
    """Die Vektordatei selbst: ist sie vollstaendig?"""

    def test_case_count(self):
        vectors = load_vectors()
        verify_cases = vectors.get("verify") or []
        hash_cases = vectors.get("hash") or []
        self.assertGreaterEqual(
            len(verify_cases), WANT_VERIFY,
            "zu wenige Unterschriftenfaelle: %d von %d erwartet" %
            (len(verify_cases), WANT_VERIFY))
        self.assertGreaterEqual(
            len(hash_cases), WANT_HASH,
            "zu wenige Hashfaelle: %d von %d erwartet" %
            (len(hash_cases), WANT_HASH))
        valid = sum(1 for case in verify_cases if case["valid"])
        invalid = len(verify_cases) - valid
        self.assertGreaterEqual(valid, WANT_VERIFY_VALID)
        self.assertGreaterEqual(invalid, WANT_VERIFY_INVALID)


class TestEd25519Verify(unittest.TestCase):
    """Die eigene Ed25519-Pruefung gegen alle Faelle der Vektordatei."""

    def test_all_cases(self):
        verify_cases = load_vectors().get("verify") or []
        self.assertGreaterEqual(len(verify_cases), WANT_VERIFY)
        seen = 0
        for case in verify_cases:
            with self.subTest(name=case["name"]):
                got = ed25519_verify.verify(
                    bytes.fromhex(case["public"]),
                    bytes.fromhex(case["message"]),
                    bytes.fromhex(case["sig"]))
                self.assertEqual(
                    got, case["valid"],
                    "%s (%s)" % (case["name"], case.get("why", "")))
            seen += 1
        self.assertEqual(seen, len(verify_cases))

    def test_s_against_group_order(self):
        """Eine Unterschrift mit S >= L ist abzuweisen. Der Fall steckt
        auch in der Vektordatei; hier wird er gebaut, damit die Regel
        nicht nur zufaellig stimmt."""
        case = next(entry for entry in load_vectors()["verify"]
                    if entry["valid"])
        public = bytes.fromhex(case["public"])
        message = bytes.fromhex(case["message"])
        signature = bytes.fromhex(case["sig"])
        self.assertTrue(ed25519_verify.verify(public, message, signature))
        # S um die Gruppenordnung erhoeht: rechnerisch derselbe Skalar,
        # aber nicht kanonisch - und darum ungueltig.
        scalar = int.from_bytes(signature[32:], "little")
        bigger = scalar + ed25519_verify.L
        if bigger < 2**256:
            bent = signature[:32] + bigger.to_bytes(32, "little")
            self.assertFalse(ed25519_verify.verify(public, message, bent))

    def test_non_canonical_point_is_rejected(self):
        """Ein oeffentlicher Schluessel muss kanonisch geschrieben sein:
        y < p. Sonst wuerden zwei verschiedene Schluessel auf denselben
        Punkt zeigen, und eine Unterschrift waere unter beiden gueltig -
        Go weist das in SetBytes ebenso ab.

        Geprueft wird hier der Dekodierer selbst: durch verify() ist die
        Regel nicht zu messen, denn beide Lesarten fuehren bei den
        vorhandenen Vektoren zu einer gescheiterten Pruefung. Ein Mutant
        auf dieser Zeile hat genau deshalb ueberlebt."""
        prime = ed25519_verify.P
        # y = p zeigt auf denselben Punkt wie y = 0, y = p + 1 auf den von
        # y = 1. Beide Schreibweisen sind unzulaessig.
        for value in (prime, prime + 1, prime + 18):
            with self.subTest(y=value):
                raw = value.to_bytes(32, "little")
                self.assertIsNone(ed25519_verify._point_decode(raw))
                self.assertFalse(
                    ed25519_verify.verify(raw, b"x", b"\x00" * 64))
        # Zur Gegenprobe: ein echter Schluessel aus der Vektordatei laesst
        # sich lesen.
        good = next(case for case in load_vectors()["verify"]
                    if case["valid"])
        self.assertIsNotNone(
            ed25519_verify._point_decode(bytes.fromhex(good["public"])))

    def test_sign_bit_on_zero_x_is_rejected(self):
        """x = 0 mit gesetztem Vorzeichenbit gibt es nicht - es waere eine
        zweite Schreibweise desselben Punktes."""
        raw = bytearray(32)
        raw[0] = 1
        raw[31] |= 0x80
        self.assertIsNone(ed25519_verify._point_decode(bytes(raw)))

    def test_wrong_lengths(self):
        """Falsche Laengen sind ein sauberes False, keine Ausnahme."""
        self.assertFalse(ed25519_verify.verify(b"", b"x", b"\x00" * 64))
        self.assertFalse(ed25519_verify.verify(b"\x00" * 32, b"x", b"\x00" * 63))
        self.assertFalse(ed25519_verify.verify(None, b"x", None))


class TestHashes(unittest.TestCase):
    """SHA-256 und SHA-512 gegen alle Hashfaelle der Vektordatei."""

    def test_all_cases(self):
        hash_cases = load_vectors().get("hash") or []
        self.assertGreaterEqual(len(hash_cases), WANT_HASH)
        seen = 0
        for case in hash_cases:
            raw = bytes.fromhex(case["input"])
            with self.subTest(length=len(raw)):
                self.assertEqual(ed25519_verify.sha256(raw).hex(),
                                 case["sha256"])
                self.assertEqual(ed25519_verify.sha512(raw).hex(),
                                 case["sha512"])
            seen += 1
        self.assertEqual(seen, len(hash_cases))


class TestAnswerReader(unittest.TestCase):
    """Der Zeilen-Leser des Drahtprotokolls."""

    def test_ok_line(self):
        answer = osk_guard.parse_answer("OK sig=aabb pub=ccdd")
        self.assertEqual(answer["sig"], "aabb")
        self.assertEqual(answer["pub"], "ccdd")

    def test_ping_line(self):
        self.assertEqual(osk_guard.parse_answer("OK v=1")["v"], "1")

    def test_error_line(self):
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.parse_answer("ERR error.guard.noecdh")
        self.assertEqual(caught.exception.code, "error.guard.noecdh")
        self.assertTrue(osk_guard.is_no_ecdh(caught.exception))

    def test_error_line_without_code(self):
        """ERR ohne Code: innerer Fehler, nicht geraten."""
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.parse_answer("ERR")
        self.assertEqual(caught.exception.code, "error.guard.internal")

    def test_broken_line(self):
        for line in ("", "   ", "KAPUTT sig=aabb", "\x00"):
            with self.subTest(line=repr(line)):
                with self.assertRaises(osk_guard.OskGuardError) as caught:
                    osk_guard.parse_answer(line)
                self.assertIn(caught.exception.code,
                              ("error.guard.answer", "error.guard.internal"))

    def test_missing_field(self):
        answer = osk_guard.parse_answer("OK pub=aabb")
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.answer_value(answer, "sig", 0)
        self.assertEqual(caught.exception.code, "error.guard.answer")

    def test_dash_is_not_a_value(self):
        answer = osk_guard.parse_answer("OK sig=-")
        with self.assertRaises(osk_guard.OskGuardError):
            osk_guard.answer_value(answer, "sig", 0)

    def test_wrong_value_length(self):
        answer = osk_guard.parse_answer("OK pub=aabb")
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.answer_value(answer, "pub", osk_guard.KEY_LEN)
        self.assertEqual(caught.exception.code, "error.guard.answer")

    def test_not_hex(self):
        answer = osk_guard.parse_answer("OK pub=zzzz")
        with self.assertRaises(osk_guard.OskGuardError):
            osk_guard.answer_value(answer, "pub", 0)

    def test_odd_hex(self):
        answer = osk_guard.parse_answer("OK pub=aab")
        with self.assertRaises(osk_guard.OskGuardError):
            osk_guard.answer_value(answer, "pub", 0)

    def test_license_line(self):
        """Die Antwort auf `license` traegt nur noch den unterschriebenen
        Block - keine fertig gelesenen Felder."""
        answer = osk_guard.parse_answer("OK lic=4f534b4c494331")
        self.assertEqual(osk_guard.answer_value(answer, "lic", 0), b"OSKLIC1")
        # Eine Antwort nach der alten Art fuehrt zu nichts: ohne lic gibt
        # es keine Werte.
        alt = osk_guard.parse_answer("OK seats=3 tiers=6261736973")
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.answer_value(alt, "lic", 0)
        self.assertEqual(caught.exception.code, "error.guard.answer")


class TestAddressCheck(unittest.TestCase):
    """Nur die Rueckschleife - und jede Laenge geprueft, bevor etwas
    gelesen wird."""

    def test_short_key(self):
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.OskGuard("127.0.0.1:8300", b"\x00" * 31)
        self.assertEqual(caught.exception.code, "error.guard.key")

    def test_foreign_address(self):
        for address in ("10.0.0.5:8300", "example.invalid:8300",
                        "[2001:db8::1]:8300"):
            with self.subTest(address=address):
                with self.assertRaises(osk_guard.OskGuardError) as caught:
                    osk_guard.OskGuard(address, b"\x00" * 32)
                self.assertEqual(caught.exception.code, "error.guard.request")

    def test_broken_address(self):
        for address in ("", "127.0.0.1", "127.0.0.1:", ":8300",
                        "127.0.0.1:port"):
            with self.subTest(address=address):
                with self.assertRaises(osk_guard.OskGuardError) as caught:
                    osk_guard.OskGuard(address, b"\x00" * 32)
                self.assertEqual(caught.exception.code, "error.guard.request")

    def test_port_out_of_range(self):
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.OskGuard("127.0.0.1:70000", b"\x00" * 32)
        self.assertEqual(caught.exception.code, "error.guard.port")

    def test_loopback_forms(self):
        for host in ("127.0.0.1", "127.9.9.9", "localhost", "::1"):
            with self.subTest(host=host):
                self.assertTrue(osk_guard._is_loopback(host))
        for host in ("10.0.0.5", "2001:db8::1", "nicht-eine-adresse"):
            with self.subTest(host=host):
                self.assertFalse(osk_guard._is_loopback(host))


class TestNodeVerify(unittest.TestCase):
    """node_verify braucht keinen Stick - nur den fremden Schluessel."""

    def test_lengths(self):
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.node_verify(b"\x00" * 31, "a", b"x", b"\x00" * 64)
        self.assertEqual(caught.exception.code, "error.guard.key")
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.node_verify(b"\x00" * 32, "a", b"x", b"\x00" * 10)
        self.assertEqual(caught.exception.code, "error.guard.signature")
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.node_verify(b"\x00" * 32, "", b"x", b"\x00" * 64)
        self.assertEqual(caught.exception.code, "error.guard.size")
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.node_verify(b"\x00" * 32, "a", b"", b"\x00" * 64)
        self.assertEqual(caught.exception.code, "error.guard.size")

    def test_wrong_signature(self):
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.node_verify(b"\x00" * 32, "a", b"x", b"\x00" * 64)
        self.assertEqual(caught.exception.code, "error.guard.signature")


class TestLicenseVectors(unittest.TestCase):
    """Die fertig unterschriebenen Lizenzen aus
    ../tests/license-vectors.json. Das sind die Faelle, die mit GUELTIGER
    Unterschrift eine falsch geschriebene Nutzlast tragen - ohne sie
    blieben die CBOR-Regeln ungemessen. Jeder Fall muss genau so
    beurteilt werden wie in der Datei vermerkt, Fehlercode eingeschlossen."""

    def test_case_count(self):
        cases = load_license_vectors().get("cases") or []
        self.assertGreaterEqual(
            len(cases), WANT_LICENSE,
            "zu wenige Lizenzfaelle: %d von %d erwartet" %
            (len(cases), WANT_LICENSE))
        valid = sum(1 for case in cases if case["valid"])
        invalid = len(cases) - valid
        self.assertGreaterEqual(valid, WANT_LICENSE_VALID)
        self.assertGreaterEqual(invalid, WANT_LICENSE_INVALID)

    def test_long_forms_are_measured(self):
        """Die drei nachgetragenen Faelle muessen in der Datei sein und
        abgewiesen werden. Ohne sie bleibt die Mindestwertpruefung der 2-,
        4- und 8-Byte-Form ungemessen; auf dem Go-Leser hat genau dort ein
        Mutant ueberlebt."""
        vectors = load_license_vectors()
        issuer = bytes.fromhex(vectors["issuerPublicKey"])
        by_name = {case["name"]: case for case in vectors.get("cases") or []}
        for name in ("zahl-in-zwei-byte-form", "zahl-in-vier-byte-form",
                     "zahl-in-acht-byte-form"):
            with self.subTest(name=name):
                self.assertIn(name, by_name)
                case = by_name[name]
                self.assertFalse(case["valid"])
                with self.assertRaises(license_reader.LicenseError) as caught:
                    license_reader.license_verify(
                        issuer, bytes.fromhex(case["blob"]))
                self.assertEqual(caught.exception.code,
                                 "error.license.format")

    def test_all_cases(self):
        vectors = load_license_vectors()
        issuer = bytes.fromhex(vectors["issuerPublicKey"])
        cases = vectors.get("cases") or []
        self.assertGreaterEqual(len(cases), WANT_LICENSE)
        seen = 0
        for case in cases:
            with self.subTest(name=case["name"]):
                self.check_case(issuer, case)
            seen += 1
        self.assertEqual(seen, len(cases))

    def check_case(self, issuer, case):
        blob = bytes.fromhex(case["blob"])
        if not case["valid"]:
            with self.assertRaises(license_reader.LicenseError) as caught:
                license_reader.license_verify(issuer, blob)
            if case.get("error"):
                self.assertEqual(caught.exception.code, case["error"],
                                 case.get("why", ""))
            return
        found = license_reader.license_verify(issuer, blob)
        want = case.get("want") or {}
        have = {
            "product": found.product,
            "holder": found.holder,
            "number": found.number,
            "tiers": found.tiers,
            "seats": found.seats,
            "issued": found.issued_utc,
            "expires": found.expires_utc,
        }
        for name, value in want.items():
            self.assertEqual(have.get(name), value,
                             "%s: %s" % (case["name"], name))


def cbor_text(text):
    """Baut einen CBOR-Text. Nur fuer die Pruefung - die Bibliothek liest
    CBOR, sie schreibt keins."""
    raw = text.encode("utf-8")
    return cbor_head(3, len(raw)) + raw


def cbor_head(major, count):
    """Baut einen CBOR-Kopf in der kuerzesten Schreibweise."""
    if count < 24:
        return bytes([(major << 5) | count])
    if count <= 0xff:
        return bytes([(major << 5) | 24, count])
    if count <= 0xffff:
        return bytes([(major << 5) | 25]) + count.to_bytes(2, "big")
    if count <= 0xffffffff:
        return bytes([(major << 5) | 26]) + count.to_bytes(4, "big")
    return bytes([(major << 5) | 27]) + count.to_bytes(8, "big")


def example_payload(issued=1700000000, expires=4070908800, seats=3):
    """Eine Nutzlast wie die der Attrappe: Produkt, Lizenznehmer, Nummer,
    zwei Ausbaustufen, Plaetze und zwei Zeitstempel. Die Zeitstempel sind
    rund 1,8 Milliarden und brauchen die vierbyteige Laengenform."""
    entries = [
        (cbor_text("p"), cbor_text("beispiel-programm")),
        (cbor_text("h"), cbor_text("Sorglos Thomas Weirich")),
        (cbor_text("nr"), cbor_text("PRUEF-0001")),
        (cbor_text("t"), cbor_head(4, 2) + cbor_text("basis") +
         cbor_text("berichte")),
        (cbor_text("s"), cbor_head(0, seats)),
        (cbor_text("i"), cbor_head(0, issued)),
        (cbor_text("e"), cbor_head(0, expires)),
    ]
    out = cbor_head(5, len(entries))
    for key, value in entries:
        out += key + value
    return out


def license_blob(payload, signature=None, version=1, magic=b"OSKLIC1",
                 size=None):
    """Baut einen OSKLIC1-Block. Die Unterschrift ist erfunden - genau
    darum muss die Bibliothek ihn abweisen."""
    if signature is None:
        signature = b"\x00" * 64
    if size is None:
        size = len(payload)
    return (magic + bytes([version]) + size.to_bytes(2, "big") + payload +
            signature)


class TestLicenseBlock(unittest.TestCase):
    """Der unterschriebene Block: Aufbau, dann Unterschrift, dann erst die
    Felder. Ein erfundener Block darf nie zu Werten fuehren."""

    ISSUER = bytes(range(32))

    def test_issuer_key_required(self):
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.license_verify(None, license_blob(
                example_payload()))
        self.assertEqual(caught.exception.code, "error.guard.issuer")
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.license_verify(b"\x00" * 31, license_blob(
                example_payload()))
        self.assertEqual(caught.exception.code, "error.guard.issuer")

    def test_wrong_magic(self):
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.license_verify(self.ISSUER, license_blob(
                example_payload(), magic=b"OSKSEAL"))
        self.assertEqual(caught.exception.code, "error.license.format")

    def test_too_short(self):
        for blob in (b"", b"OSKLIC1", b"OSKLIC1\x01\x00\x01" + b"x"):
            with self.subTest(length=len(blob)):
                with self.assertRaises(license_reader.LicenseError) as caught:
                    license_reader.license_verify(self.ISSUER, blob)
                self.assertEqual(caught.exception.code,
                                 "error.license.format")

    def test_unknown_version(self):
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.license_verify(self.ISSUER, license_blob(
                example_payload(), version=2))
        self.assertEqual(caught.exception.code, "error.license.version")

    def test_length_must_match_exactly(self):
        payload = example_payload()
        for size in (0, len(payload) - 1, len(payload) + 1):
            with self.subTest(size=size):
                with self.assertRaises(license_reader.LicenseError) as caught:
                    license_reader.license_verify(
                        self.ISSUER, license_blob(payload, size=size))
                self.assertEqual(caught.exception.code,
                                 "error.license.format")

    def test_appended_bytes(self):
        """Angehaengte Bytes werden nicht stillschweigend uebergangen."""
        payload = example_payload()
        blob = license_blob(payload) + b"noch etwas"
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.license_verify(self.ISSUER, blob)
        self.assertEqual(caught.exception.code, "error.license.format")

    def test_invented_signature_is_rejected(self):
        """Das ist der Kern des Umbaus: ein Block mit erfundener
        Unterschrift gibt keine Felder heraus, sondern einen Fehler."""
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.license_verify(self.ISSUER,
                                          license_blob(example_payload()))
        self.assertEqual(caught.exception.code, "error.license.signature")


class TestLicenseFields(unittest.TestCase):
    """Das Lesen der GEPRUEFTEN Nutzlast, ohne Unterschrift."""

    def test_full_payload(self):
        found = license_reader.license_fields(example_payload())
        self.assertEqual(found.product, "beispiel-programm")
        self.assertEqual(found.holder, "Sorglos Thomas Weirich")
        self.assertEqual(found.number, "PRUEF-0001")
        self.assertEqual(found.tiers, ["basis", "berichte"])
        self.assertEqual(found.seats, 3)
        self.assertEqual(found.issued_utc, 1700000000)
        self.assertEqual(found.expires_utc, 4070908800)
        self.assertFalse(found.expired())
        self.assertTrue(found.has_tier("berichte"))

    def test_four_byte_timestamp(self):
        """Ein Zeitstempel in Sekunden passt nicht in zwei Byte. Genau
        hier hat ein zu kurzer Leser die Lizenz abgewiesen."""
        payload = example_payload(issued=1700000000)
        self.assertIn(bytes([0x1a]), payload)
        self.assertEqual(
            license_reader.license_fields(payload).issued_utc, 1700000000)

    def test_eight_byte_number(self):
        big = 2**33
        payload = example_payload(issued=big, expires=0)
        found = license_reader.license_fields(payload)
        self.assertEqual(found.issued_utc, big)
        self.assertEqual(found.expires_utc, 0)
        self.assertFalse(found.expired())

    def test_missing_product(self):
        payload = (cbor_head(5, 1) + cbor_text("h") +
                   cbor_text("ohne Produkt"))
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.license_fields(payload)
        self.assertEqual(caught.exception.code, "error.license.product")

    def test_empty_tier(self):
        payload = (cbor_head(5, 2) + cbor_text("p") + cbor_text("x") +
                   cbor_text("t") + cbor_head(4, 1) + cbor_text(""))
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.license_fields(payload)
        self.assertEqual(caught.exception.code, "error.license.tier")

    def test_time_out_of_range(self):
        payload = example_payload(issued=2**63)
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.license_fields(payload)
        self.assertEqual(caught.exception.code, "error.license.time")


class TestCborReader(unittest.TestCase):
    """Der CBOR-Leser: nur was eine Lizenz braucht, und jede Laenge
    geprueft, bevor gelesen wird."""

    def test_shortest_form_is_enforced(self):
        # Dieselbe Zahl in einer zu langen Schreibweise: abgewiesen, denn
        # in unterschriebenen Daten darf es nur eine Schreibweise geben.
        for raw in (bytes([0x18, 0x05]),
                    bytes([0x19, 0x00, 0x05]),
                    bytes([0x1a, 0x00, 0x00, 0x00, 0x05]),
                    bytes([0x1b]) + (5).to_bytes(8, "big"),
                    bytes([0x19, 0x00, 0xff]),
                    bytes([0x1a, 0x00, 0x00, 0xff, 0xff]),
                    bytes([0x1b]) + (0xffffffff).to_bytes(8, "big")):
            with self.subTest(form=raw.hex()):
                payload = cbor_head(5, 1) + cbor_text("s") + raw
                with self.assertRaises(license_reader.LicenseError):
                    license_reader.cbor_map(payload)

    def test_indefinite_and_reserved(self):
        for low in (28, 29, 30, 31):
            with self.subTest(low=low):
                with self.assertRaises(license_reader.LicenseError):
                    license_reader.cbor_map(bytes([(5 << 5) | low]))

    def test_unused_major_types(self):
        # 1 negative Zahl, 2 Bytefolge, 6 Markierung, 7 einfacher Wert.
        for major in (1, 2, 6, 7):
            with self.subTest(major=major):
                payload = cbor_head(5, 1) + cbor_text("s") + \
                    bytes([(major << 5) | 1])
                with self.assertRaises(license_reader.LicenseError):
                    license_reader.cbor_map(payload)

    def test_count_larger_than_buffer(self):
        for payload in (cbor_head(3, 20) + b"kurz",
                        cbor_head(4, 200) + b"kurz",
                        cbor_head(5, 200) + b"kurz"):
            with self.subTest(payload=payload[:4].hex()):
                with self.assertRaises(license_reader.LicenseError) as caught:
                    license_reader.cbor_map(payload)
                self.assertEqual(caught.exception.code,
                                 "error.license.format")

    def test_key_twice(self):
        payload = (cbor_head(5, 2) + cbor_text("p") + cbor_text("eins") +
                   cbor_text("p") + cbor_text("zwei"))
        with self.assertRaises(license_reader.LicenseError) as caught:
            license_reader.cbor_map(payload)
        self.assertEqual(caught.exception.code, "error.license.format")

    def test_key_must_be_text(self):
        payload = cbor_head(5, 1) + cbor_head(0, 1) + cbor_text("x")
        with self.assertRaises(license_reader.LicenseError):
            license_reader.cbor_map(payload)

    def test_leftover_bytes(self):
        with self.assertRaises(license_reader.LicenseError):
            license_reader.cbor_map(cbor_head(5, 0) + b"\x00")

    def test_not_a_map(self):
        with self.assertRaises(license_reader.LicenseError):
            license_reader.cbor_map(cbor_text("nur ein Text"))

    def test_too_deep(self):
        payload = cbor_head(4, 1) * 8 + cbor_head(0, 1)
        with self.assertRaises(license_reader.LicenseError):
            license_reader.cbor_map(payload)

    def test_empty(self):
        with self.assertRaises(license_reader.LicenseError):
            license_reader.cbor_map(b"")


class FakeAgent(threading.Thread):
    """Ein kleiner falscher Dienst auf der Rueckschleife - der Angreifer,
    gegen den der Umbau gemacht ist. Er antwortet, was man ihm vorgibt,
    und die Bibliothek darf ihm trotzdem nichts glauben."""

    def __init__(self, answers):
        super().__init__(daemon=True)
        self.answers = answers
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.bind(("127.0.0.1", 0))
        self.socket.listen(8)
        self.address = "127.0.0.1:%d" % self.socket.getsockname()[1]
        self.running = True

    def run(self):
        while self.running:
            try:
                connection, _ = self.socket.accept()
            except OSError:
                return
            try:
                line = connection.recv(1 << 20).decode("ascii", "replace")
                verb = line.split()[0] if line.split() else ""
                answer = self.answers.get(verb, "ERR error.guard.verb")
                connection.sendall(answer.encode("ascii") + b"\n")
            except OSError:
                pass
            finally:
                connection.close()

    def close(self):
        self.running = False
        try:
            self.socket.close()
        except OSError:
            pass


class TestAgainstFakeAgent(unittest.TestCase):
    """Die Bibliothek gegen einen Dienst, der luegt."""

    def setUp(self):
        payload = example_payload()
        self.agent = FakeAgent({
            "ping": "OK v=1",
            "license": "OK lic=" + license_blob(payload).hex(),
        })
        self.agent.start()
        self.addCleanup(self.agent.close)
        self.device_key = bytes(range(32))
        self.issuer_key = bytes(range(32, 64))

    def test_invented_license_is_rejected(self):
        guard = osk_guard.OskGuard(self.agent.address, self.device_key,
                                   self.issuer_key)
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            guard.license("beispiel-programm")
        self.assertEqual(caught.exception.code, "error.license.signature")

    def test_without_issuer_key(self):
        """Ohne Herstellerschluessel kommt ein Fehler und kein Wert - es
        gibt bewusst keinen Weg zu ungepruefter Auskunft."""
        guard = osk_guard.OskGuard(self.agent.address, self.device_key)
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            guard.license("beispiel-programm")
        self.assertEqual(caught.exception.code, "error.guard.issuer")

    def test_short_issuer_key(self):
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            osk_guard.OskGuard(self.agent.address, self.device_key,
                               b"\x00" * 31)
        self.assertEqual(caught.exception.code, "error.guard.issuer")

    def test_missing_license(self):
        agent = FakeAgent({"ping": "OK v=1",
                           "license": "ERR error.license.missing"})
        agent.start()
        self.addCleanup(agent.close)
        guard = osk_guard.OskGuard(agent.address, self.device_key,
                                   self.issuer_key)
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            guard.license("gibtesnicht")
        self.assertEqual(caught.exception.code, "error.license.missing")

    def test_wrong_device(self):
        """Antwortet ein anderer Stick, heisst der Fehler
        error.guard.wrongdevice und nicht nur "Unterschrift falsch" - der
        Nutzer soll lesen, dass der falsche Stick steckt. Ohne diesen Fall
        waere der Unterschied ungemessen: abgewiesen wuerde es ohnehin,
        aber mit dem falschen Satz."""
        other = bytes(range(64, 96))
        agent = FakeAgent({
            "ping": "OK v=1",
            "attest": "OK sig=" + ("00" * 64) + " pub=" + other.hex(),
        })
        agent.start()
        self.addCleanup(agent.close)
        guard = osk_guard.OskGuard(agent.address, self.device_key,
                                   self.issuer_key)
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            guard.present(__file__)
        self.assertEqual(caught.exception.code, "error.guard.wrongdevice")

    def test_right_device_wrong_signature(self):
        """Der richtige Schluessel, aber eine erfundene Unterschrift: der
        Dienst sagt Ja, die Bibliothek glaubt es nicht."""
        agent = FakeAgent({
            "ping": "OK v=1",
            "attest": "OK sig=" + ("00" * 64) + " pub=" +
                      self.device_key.hex(),
        })
        agent.start()
        self.addCleanup(agent.close)
        guard = osk_guard.OskGuard(agent.address, self.device_key,
                                   self.issuer_key)
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            guard.present(__file__)
        self.assertEqual(caught.exception.code, "error.guard.signature")

    def test_unknown_verb(self):
        guard = osk_guard.OskGuard(self.agent.address, self.device_key,
                                   self.issuer_key)
        with self.assertRaises(osk_guard.OskGuardError) as caught:
            guard.node_sign("auftraege", b"x")
        self.assertEqual(caught.exception.code, "error.guard.verb")


class TestLicense(unittest.TestCase):
    """Ablauf und Ausbaustufen."""

    def test_expired(self):
        self.assertFalse(osk_guard.License(expires_utc=0).expired())
        self.assertTrue(osk_guard.License(expires_utc=1).expired())
        self.assertFalse(osk_guard.License(expires_utc=4070908800).expired())

    def test_has_tier(self):
        found = osk_guard.License(tiers=["basis", "Berichte"])
        self.assertTrue(found.has_tier("berichte"))
        self.assertTrue(found.has_tier("BASIS"))
        self.assertFalse(found.has_tier("gibtesnicht"))


class TestWipe(unittest.TestCase):
    """Geheimnisse loeschen, soweit Python das zulaesst."""

    def test_bytearray_is_cleared(self):
        secret = bytearray(b"geheim")
        osk_guard.wipe(secret)
        self.assertEqual(bytes(secret), b"\x00" * 6)

    def test_bytes_are_left_alone(self):
        # Unveraenderliche bytes kann Python nicht ueberschreiben; wipe
        # tut dann nichts und behauptet auch nichts.
        osk_guard.wipe(b"geheim")


if __name__ == "__main__":
    unittest.main()
