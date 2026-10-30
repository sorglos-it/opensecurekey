"""osk-guard fuer Python: ein geschuetztes Programm in wenigen Zeilen.

    guard = OskGuard("127.0.0.1:8300", device_key, issuer_key)
    guard.present()                                  # eine Runde Frage/Antwort

Die Bibliothek haengt am lokalen Dienst des Programms `osk` und prueft die
Unterschrift SELBST. Dem Dienst wird kein Ja geglaubt - sonst waere der
Schutz die Aussage eines Programms ueber sich selbst. Das gilt fuer die
Praesenz und seit dem Umbau auch fuer die Lizenz: der Dienst gibt nur den
unterschriebenen Block heraus.

Nur Standardbibliothek, kein Fremdcode. Ed25519 steckt in
ed25519_verify.py, dort nur als Pruefung; der Lizenzblock wird in
license_reader.py gelesen.
"""

import hashlib
import os
import secrets
import socket
import sys
import time

import ed25519_verify
import license_reader

# Die Lizenz wird HIER geprueft, nicht vom Dienst. License kommt aus
# license_reader, damit Aufrufer nur osk_guard kennen muessen.
License = license_reader.License
license_verify = license_reader.license_verify

# Groessen des Verfahrens - dieselben wie im Geraet.
NONCE_LEN = 32
HASH_LEN = 32
KEY_LEN = 32
SIG_LEN = 64
SECRET_LEN = 32

ATTEST_DOMAIN = b"osk-guard/attest/1"
SIGN_DOMAIN = b"osk-guard/sign/1"

MAX_BINARY = 512 * 1024 * 1024
MAX_LINE = 4 * 1024 * 1024
MAX_PLAIN = 1024 * 1024

_CONNECT_TIMEOUT = 3.0
_ANSWER_TIMEOUT = 30.0
_READ_CHUNK = 64 * 1024


class OskGuardError(Exception):
    """Fehler des Dongle-Schutzes. code ist der Schluessel, zu dem in
    lang/<sprache>.json ein Satz steht - derselbe Schluessel in allen fuenf
    Bibliotheken."""

    def __init__(self, code):
        super().__init__(code)
        self.code = code


def _fail(code):
    return OskGuardError(code)


def code_of(error):
    """Nennt den Fehlerschluessel, oder "" bei einem fremden Fehler."""
    if isinstance(error, OskGuardError):
        return error.code
    return ""


def is_no_ecdh(error):
    """Sagt, ob der gelieferte Chip keinen Schluesselaustausch kann. Dann
    faellt der Serverbetrieb mit Schluessel im Chip weg - das Programm
    sollte den Anwendungsfall abschalten und es sagen, nicht abbrechen."""
    return code_of(error) == "error.guard.noecdh"


def wipe(buffer):
    """Ueberschreibt einen bytearray mit Nullen. Fuer unveraenderliche
    bytes-Objekte und fuer ganze Zahlen geht das in Python nicht - diese
    Werte liegen im Speicher, bis die Speicherverwaltung sie einsammelt.
    Das ist eine Grenze der Sprache und keine Nachlaessigkeit; darum wird
    hier nichts behauptet, was nicht stimmt."""
    if isinstance(buffer, bytearray):
        for index in range(len(buffer)):
            buffer[index] = 0


def _hex_text(text):
    return text.encode("utf-8").hex()


def _hex_or_dash(raw):
    if not raw:
        return "-"
    return bytes(raw).hex()


def _split_host_port(address):
    """Trennt Rechner und Port. Auch die Schreibweise [::1]:8300 fuer
    IPv6. Gibt (None, None), wenn die Angabe nicht passt."""
    if not isinstance(address, str) or not address:
        return None, None
    if address.startswith("["):
        end = address.find("]")
        if end < 0 or not address[end + 1:].startswith(":"):
            return None, None
        return address[1:end], address[end + 2:]
    if address.count(":") != 1:
        return None, None
    host, _, port = address.partition(":")
    return host, port


def _is_loopback(host):
    """Nur die Rueckschleife: der Stick steckt hier, nicht im Netz. Es
    wird nicht aufgeloest - ein Name, der heute auf 127.0.0.1 zeigt, kann
    morgen woanders zeigen."""
    if host == "localhost":
        return True
    for family in (socket.AF_INET, socket.AF_INET6):
        try:
            packed = socket.inet_pton(family, host)
        except (OSError, ValueError):
            continue
        if family == socket.AF_INET:
            return packed[0] == 127
        if packed == b"\x00" * 15 + b"\x01":
            return True
        # Eine in IPv6 eingebettete IPv4-Adresse: ::ffff:127.0.0.1
        if packed[:12] == b"\x00" * 10 + b"\xff\xff":
            return packed[12] == 127
        return False
    return False


def parse_answer(line):
    """Liest eine Antwortzeile des Dienstes. OK gibt die Felder als
    Abbildung, ERR wird zur Ausnahme mit dem gemeldeten Code."""
    if isinstance(line, (bytes, bytearray)):
        line = bytes(line).decode("ascii", "replace")
    fields = line.split()
    if not fields:
        raise _fail("error.guard.answer")
    if fields[0] == "ERR":
        if len(fields) < 2:
            raise _fail("error.guard.internal")
        raise _fail(fields[1])
    if fields[0] != "OK":
        raise _fail("error.guard.answer")
    answer = {}
    for field in fields[1:]:
        name, sign, value = field.partition("=")
        if sign:
            answer[name] = value
    return answer


def answer_value(answer, key, want):
    """Holt einen Hexwert aus der Antwort und prueft seine Laenge. want 0
    heisst: jede Laenge ist recht."""
    text = answer.get(key)
    if text is None or text == "-":
        raise _fail("error.guard.answer")
    if len(text) % 2 != 0:
        raise _fail("error.guard.answer")
    try:
        raw = bytes.fromhex(text)
    except ValueError:
        raise _fail("error.guard.answer") from None
    if want > 0 and len(raw) != want:
        raise _fail("error.guard.answer")
    return raw


def binary_hash(path=""):
    """Pruefsumme einer Programmdatei. Leerer Pfad: die laufende Datei."""
    if not path:
        # Bei einem gepackten Programm ist das die EXE, sonst das Skript.
        path = sys.executable if getattr(sys, "frozen", False) else sys.argv[0]
        if not path:
            raise _fail("error.guard.binary")
    try:
        if not os.path.isfile(path):
            raise _fail("error.guard.binary")
        if os.path.getsize(path) > MAX_BINARY:
            raise _fail("error.guard.binary")
        digest = hashlib.sha256()
        read = 0
        with open(path, "rb") as handle:
            while True:
                chunk = handle.read(_READ_CHUNK)
                if not chunk:
                    break
                read += len(chunk)
                if read > MAX_BINARY:
                    raise _fail("error.guard.binary")
                digest.update(chunk)
        return digest.digest()
    except OSError:
        raise _fail("error.guard.binary") from None


def _message(domain, nonce, bin_hash):
    """Baut die Bytefolge, die das Geraet bei der Praesenz unterschreibt."""
    return domain + b"\x00" + bytes(nonce) + bytes(bin_hash)


def node_verify(public_key, label, job, signature):
    """Prueft die Unterschrift eines anderen Knotens. Das braucht keinen
    Stick - nur den oeffentlichen Schluessel der Gegenseite. Bei einer
    falschen Unterschrift kommt eine Ausnahme, kein stilles False."""
    if public_key is None or len(public_key) != KEY_LEN:
        raise _fail("error.guard.key")
    if signature is None or len(signature) != SIG_LEN:
        raise _fail("error.guard.signature")
    if not label or not job:
        raise _fail("error.guard.size")
    job_hash = hashlib.sha256(bytes(job)).digest()
    signed = SIGN_DOMAIN + b"\x00" + label.encode("utf-8") + b"\x00" + job_hash
    if not ed25519_verify.verify(public_key, signed, signature):
        raise _fail("error.guard.signature")


class OskGuard:
    """Die Verbindung zum lokalen Dienst."""

    def __init__(self, address, device_public_key, issuer_public_key=None,
                 timeout=_ANSWER_TIMEOUT):
        """Prueft die Angaben und sieht nach, ob der Dienst antwortet.

        device_public_key sind die 32 Byte, die der Hersteller einmal mit
        `osk guard-key` aus seinem Stick gelesen und ins Programm
        eingetragen hat.

        issuer_public_key ist der oeffentliche Schluessel des Herstellers,
        gegen den die Lizenz geprueft wird. Er darf fehlen; dann scheitert
        license() mit error.guard.issuer, statt etwas Ungeprueftes zu
        liefern."""
        if device_public_key is None or len(device_public_key) != KEY_LEN:
            raise _fail("error.guard.key")
        if issuer_public_key and len(issuer_public_key) != KEY_LEN:
            raise _fail("error.guard.issuer")
        host, port = _split_host_port(address)
        if not host or not port or not port.isdigit():
            raise _fail("error.guard.request")
        if not 1 <= int(port) <= 65535:
            raise _fail("error.guard.port")
        if not _is_loopback(host):
            raise _fail("error.guard.request")
        self.address = address
        self._host = host
        self._port = int(port)
        self._device_pub = bytes(device_public_key)
        self._issuer_pub = bytes(issuer_public_key) if issuer_public_key else b""
        if timeout and timeout > 0:
            self.timeout = float(timeout)
        else:
            self.timeout = _ANSWER_TIMEOUT
        self._ask("ping")

    @classmethod
    def open(cls, address, device_public_key, issuer_public_key=None,
             timeout=_ANSWER_TIMEOUT):
        """Gleichbedeutend mit dem Konstruktor - fuer Programme, die sich
        an die Schreibweise der anderen vier Bibliotheken halten."""
        return cls(address, device_public_key, issuer_public_key, timeout)

    def _ask(self, line):
        """Schickt eine Zeile und liest die Antwort. Je Anfrage eine
        Verbindung: das ist langsamer, aber ein Programm kann so nie die
        Antwort einer fremden Anfrage lesen."""
        if len(line) + 1 > MAX_LINE:
            raise _fail("error.guard.size")
        try:
            connection = socket.create_connection(
                (self._host, self._port), timeout=_CONNECT_TIMEOUT)
        except OSError:
            raise _fail("error.guard.nodevice") from None
        try:
            # Zeitlimit auf jedem Netzaufruf: nie unbegrenzt warten.
            connection.settimeout(self.timeout)
            connection.sendall(line.encode("ascii") + b"\n")
            answer = self._read_line(connection)
        except OSError:
            raise _fail("error.guard.nodevice") from None
        finally:
            try:
                connection.close()
            except OSError:
                pass
        return parse_answer(answer)

    @staticmethod
    def _read_line(connection):
        """Liest genau eine Zeile mit Obergrenze. Ohne die Grenze wuerde
        ein Dienst, der nie ein Zeilenende schickt, beliebig viel Speicher
        belegen."""
        parts = bytearray()
        while True:
            chunk = connection.recv(_READ_CHUNK)
            if not chunk:
                raise _fail("error.guard.nodevice")
            end = chunk.find(b"\n")
            if end >= 0:
                if len(parts) + end > MAX_LINE:
                    raise _fail("error.guard.size")
                parts += chunk[:end]
                break
            if len(parts) + len(chunk) > MAX_LINE:
                raise _fail("error.guard.size")
            parts += chunk
        return bytes(parts).rstrip(b"\r").decode("ascii", "replace")

    def present(self, binary_path=""):
        """Eine Runde Frage und Antwort: der Zufallswert kommt von hier,
        die Unterschrift wird hier geprueft. Kommt kein Fehler, steckte in
        diesem Augenblick der richtige Stick, und die Programmdatei ist
        unveraendert.

        binary_path leer heisst: die laufende Datei. Was diese Stufe wert
        ist, steht in docs/dongle.md - sie haelt das Mitnehmen einer Kopie
        auf und nichts darueber hinaus. Echter Schutz ist seal/unseal."""
        bin_hash = binary_hash(binary_path)
        try:
            nonce = secrets.token_bytes(NONCE_LEN)
        except (OSError, NotImplementedError):
            raise _fail("error.guard.random") from None
        if len(nonce) != NONCE_LEN:
            raise _fail("error.guard.random")
        answer = self._ask("attest " + nonce.hex() + " " + bin_hash.hex())
        public = answer_value(answer, "pub", KEY_LEN)
        signature = answer_value(answer, "sig", SIG_LEN)
        # Gleiche Laufzeit, damit der Vergleich nichts verraet.
        if not secrets.compare_digest(public, self._device_pub):
            raise _fail("error.guard.wrongdevice")
        if not ed25519_verify.verify(self._device_pub,
                                     _message(ATTEST_DOMAIN, nonce, bin_hash),
                                     signature):
            raise _fail("error.guard.signature")

    def watch(self, every=2.0, binary_path="", lost=None, rounds=0):
        """Fragt in festem Abstand weiter und ruft lost auf, sobald eine
        Runde scheitert - beim Abziehen also beim naechsten Durchgang.
        Danach kehrt watch zurueck. Ein geschuetztes Programm beendet sich
        in lost. rounds > 0 begrenzt die Durchgaenge, damit ein Beispiel
        von selbst endet."""
        if not every or every <= 0:
            every = 2.0
        done = 0
        while True:
            time.sleep(every)
            try:
                self.present(binary_path)
            except OskGuardError as error:
                if lost is not None:
                    lost(error)
                return
            done += 1
            if rounds and done >= rounds:
                return

    def seal(self, label, extra, plain):
        """Verschliesst Daten mit einem Schluessel, der den Stick nie
        verlaesst. extra darf leer sein; ist er gesetzt, muss er beim
        Oeffnen gleich sein."""
        if not plain:
            raise _fail("error.guard.size")
        if len(plain) > MAX_PLAIN:
            raise _fail("error.guard.size")
        answer = self._ask("seal " + _hex_text(label) + " " +
                           _hex_or_dash(extra) + " " + bytes(plain).hex())
        return answer_value(answer, "blob", 0)

    def unseal(self, label, extra, blob):
        """Macht wieder auf. Ohne Stick gibt es den Schluessel nicht -
        dann kommt ein Fehler und keine halben Daten."""
        if not blob:
            raise _fail("error.guard.size")
        # Kopf und Siegel des Formats OSKSEAL1 kommen zur Nutzmenge dazu.
        if len(blob) > MAX_PLAIN + 4096:
            raise _fail("error.guard.size")
        answer = self._ask("unseal " + _hex_text(label) + " " +
                           _hex_or_dash(extra) + " " + bytes(blob).hex())
        return answer_value(answer, "plain", 0)

    def seal_code(self, label, code):
        """Verschliesst einen Programmteil. Der erwartete Hash geht als
        zusaetzlich gepruefte Daten mit hinein, damit ein Behaelter fuer
        einen anderen Teil sich hier nicht oeffnen laesst. Zurueck kommen
        Behaelter und Hash; der Hash wird ins Programm eingetragen."""
        if not code:
            raise _fail("error.guard.size")
        code_hash = hashlib.sha256(bytes(code)).digest()
        return self.seal(label, code_hash, code), code_hash

    def unseal_code(self, label, blob, code_hash):
        """Macht einen verschluesselten Programmteil auf und haelt ihn
        gegen den Hash, den der Hersteller eingetragen hat. Das ist der
        starke Weg, und er ist der bequemste: es gibt keinen Aufruf, der
        nur Ja sagt.

        Ohne Stick kommt ein Fehler und KEINE Bytes - der Programmteil ist
        dann nicht da, nicht bloss abgelehnt. Genau darin unterscheidet
        sich Stufe 2 von present: hier gibt es keinen Zweig, den jemand
        herausschneiden kann."""
        if code_hash is None or len(code_hash) != HASH_LEN:
            raise _fail("error.guard.codehash")
        code = self.unseal(label, code_hash, blob)
        if not secrets.compare_digest(hashlib.sha256(code).digest(),
                                      bytes(code_hash)):
            raise _fail("error.guard.codehash")
        return code

    def license(self, product):
        """Holt den unterschriebenen Lizenzblock und prueft ihn SELBST.

        Der Dienst gibt nur `lic=<hex>` heraus - den ganzen OSKLIC1-Block.
        Geprueft wird in dieser Reihenfolge: Aufbau, Unterschrift des
        Herstellers, und erst dann werden die Felder aus der geprueften
        Nutzlast gelesen. Frueher lieferte der Dienst die fertigen Felder;
        dann haette ein falscher Dienst auf der Rueckschleife Lizenzen
        erfinden koennen, ohne das geschuetzte Programm anzufassen - eine
        niedrigere Huerde als ein Disassembler.

        Ohne eingetragenen Herstellerschluessel kommt
        error.guard.issuer. Es gibt bewusst keinen Weg, der ungeprueft zu
        Werten fuehrt."""
        if len(self._issuer_pub) != KEY_LEN:
            raise _fail("error.guard.issuer")
        answer = self._ask("license " + _hex_text(product))
        blob = answer_value(answer, "lic", 0)
        try:
            return license_reader.license_verify(self._issuer_pub, blob)
        except license_reader.LicenseError as error:
            # Eine Sorte Ausnahme nach draussen, derselbe Code wie in
            # lang/<sprache>.json.
            raise _fail(error.code) from None

    def node_public(self, label, curve):
        """Holt den oeffentlichen Teil eines Knotenschluessels. curve ist
        "ed25519" zum Unterschreiben oder "x25519" zum
        Schluesselaustausch."""
        if curve not in ("ed25519", "x25519"):
            raise _fail("error.guard.curve")
        answer = self._ask("nodepub " + _hex_text(label) + " " +
                           _hex_text(curve))
        return answer_value(answer, "pub", KEY_LEN)

    def node_sign(self, label, job):
        """Laesst den Knoten einen Auftrag unterschreiben."""
        if not job:
            raise _fail("error.guard.size")
        answer = self._ask("nodesign " + _hex_text(label) + " " +
                           bytes(job).hex())
        return answer_value(answer, "sig", SIG_LEN)

    def node_agree(self, label, peer_public):
        """Handelt ein gemeinsames Geheimnis mit der Gegenseite aus. Kann
        der gelieferte Chip kein ECDH, kommt error.guard.noecdh - siehe
        is_no_ecdh."""
        if peer_public is None or len(peer_public) != SECRET_LEN:
            raise _fail("error.guard.peerkey")
        answer = self._ask("nodeagree " + _hex_text(label) + " " +
                           bytes(peer_public).hex())
        # Als bytearray, damit der Aufrufer das Geheimnis loeschen kann.
        return bytearray(answer_value(answer, "sec", SECRET_LEN))
