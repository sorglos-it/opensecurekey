"""Beispiel in Python: ein Programm, das nur mit Stick laeuft, eine Datei
verschliesst, die ohne Stick unlesbar bleibt, seine Lizenz prueft und einen
Auftrag unterschreibt.

    python example.py --stand stand.json

Jede Zeile, die mit "ergebnis:" anfaengt, liest der Pruefstand mit. Bricht
das Beispiel ab, ist der Ausstiegscode nicht 0.
"""

import argparse
import json
import sys
import threading

import osk_guard


def read_stand(path):
    """Liest die Datei des Pruefstands: Adresse, Geraeteschluessel,
    Herstellerschluessel, Produkt, Bezeichnung und Knotenschluessel."""
    with open(path, "r", encoding="utf-8") as handle:
        stand = json.load(handle)
    for name in ("address", "deviceKey", "issuerKey", "product", "label",
                 "nodeKey"):
        if not stand.get(name):
            raise ValueError("im Stand fehlt " + name)
    device_key = bytes.fromhex(stand["deviceKey"])
    if len(device_key) != osk_guard.KEY_LEN:
        raise ValueError("der Geraeteschluessel hat nicht 32 Byte")
    issuer_key = bytes.fromhex(stand["issuerKey"])
    if len(issuer_key) != osk_guard.KEY_LEN:
        raise ValueError("der Herstellerschluessel hat nicht 32 Byte")
    return stand, device_key, issuer_key


def step_present(stand, device_key, issuer_key):
    """1. Ohne Stick laeuft das Programm nicht los. Der
    Herstellerschluessel wird gleich mit eingetragen - ohne ihn gaebe es
    spaeter keine gepruefte Lizenz."""
    guard = osk_guard.OskGuard.open(stand["address"], device_key, issuer_key)
    guard.present("")
    print("ergebnis: praesenz ok")
    return guard


def step_seal(guard, label):
    """2. Eine Datei, die ohne Stick unlesbar bleibt - und die mit dem
    falschen Zusatz nicht aufgeht."""
    plain = "die Tabelle, ohne die das Programm nichts kann".encode("utf-8")
    blob = guard.seal(label, b"version-1", plain)
    back = guard.unseal(label, b"version-1", blob)
    if back != plain:
        raise RuntimeError("heraus kam etwas anderes")
    # Mit der falschen Programmversion geht sie nicht auf - das muss so sein.
    try:
        guard.unseal(label, b"version-2", blob)
    except osk_guard.OskGuardError:
        pass
    else:
        raise RuntimeError("mit falschem Zusatz aufgegangen")
    print("ergebnis: verschlossen %d byte, wieder offen" % len(blob))


def step_seal_code(guard, label):
    """2b. Ein verschluesselter PROGRAMMTEIL - der starke Weg: ohne Stick
    fehlt dem Angreifer nicht die Antwort, sondern der Teil selbst."""
    code = "die Regeln, nach denen dieses Programm rechnet".encode("utf-8")
    code_blob, code_hash = guard.seal_code(label, code)
    back = guard.unseal_code(label, code_blob, code_hash)
    if back != code:
        raise RuntimeError("der Programmteil kam anders zurueck")
    # Mit einem anderen Hash gibt es KEINE Bytes.
    other = bytearray(code_hash)
    other[0] ^= 0x01
    try:
        guard.unseal_code(label, code_blob, bytes(other))
    except osk_guard.OskGuardError as error:
        if not error.code:
            raise RuntimeError("kein Fehlerschluessel")
    else:
        raise RuntimeError("ein fremder Programmteil ging auf")
    print("ergebnis: programmteil %d byte, nur mit stick lesbar"
          % len(code_blob))


def step_license(guard, product):
    """3. Die Lizenz - die Bibliothek prueft die Unterschrift des
    Herstellers selbst und liest die Felder erst danach."""
    license_found = guard.license(product)
    if license_found.expired():
        raise RuntimeError("Lizenz abgelaufen")
    if not license_found.has_tier("berichte"):
        raise RuntimeError("Ausbaustufe fehlt")
    print("ergebnis: lizenz %s, %d plaetze, stufen %s" %
          (license_found.product, license_found.seats,
           ", ".join(license_found.tiers)))


def step_sign(guard, node_label):
    """4. Ein Auftrag, unterschrieben vom Schluessel im Chip - und ein
    veraendertes Byte, das auffallen muss."""
    public = guard.node_public(node_label, "ed25519")
    job = b'{"tat":"abbild-holen","wann":1700000000}'
    signature = guard.node_sign(node_label, job)
    osk_guard.node_verify(public, node_label, job, signature)
    bent = bytearray(job)
    bent[3] ^= 1
    try:
        osk_guard.node_verify(public, node_label, bytes(bent), signature)
    except osk_guard.OskGuardError:
        pass
    else:
        raise RuntimeError("ein veraenderter Auftrag galt")
    print("ergebnis: auftrag unterschrieben und geprueft")


def step_agree(guard, node_label):
    """5. Der Schluesselaustausch - und der Fall, dass der Chip ihn nicht
    kann. Dann faellt ein Anwendungsfall weg; das wird benannt und nicht
    zum Abbruch."""
    missing = ("ergebnis: kein schluesselaustausch - "
               "serverbetrieb mit schluessel im chip faellt weg")
    try:
        peer = guard.node_public(node_label, "x25519")
    except osk_guard.OskGuardError as error:
        if not osk_guard.is_no_ecdh(error):
            raise
        print(missing)
        return
    secret = None
    try:
        secret = guard.node_agree(node_label, peer)
    except osk_guard.OskGuardError as error:
        if not osk_guard.is_no_ecdh(error):
            raise
        print(missing)
        return
    try:
        print("ergebnis: gemeinsames geheimnis %d byte" % len(secret))
    finally:
        # Das Geheimnis wird nicht gebraucht: sofort ueberschreiben.
        osk_guard.wipe(secret)


def step_watch(guard):
    """6. Weiterlaufen und beim Abziehen abbrechen - hier nur wenige
    Runden, damit das Beispiel von selbst endet."""
    broken = []
    watcher = threading.Thread(
        target=guard.watch,
        kwargs={"every": 0.2, "binary_path": "",
                "lost": broken.append, "rounds": 3},
        daemon=True)
    watcher.start()
    watcher.join(5.0)
    if broken:
        raise RuntimeError("abgebrochen: " + broken[0].code)
    if watcher.is_alive():
        raise RuntimeError("die Ueberwachung antwortet nicht")
    print("ergebnis: ueberwachung laeuft")


def run(path):
    stand, device_key, issuer_key = read_stand(path)
    guard = step_present(stand, device_key, issuer_key)
    step_seal(guard, stand["label"])
    step_seal_code(guard, stand["label"])
    step_license(guard, stand["product"])
    step_sign(guard, stand["nodeKey"])
    step_agree(guard, stand["nodeKey"])
    step_watch(guard)


def main():
    parser = argparse.ArgumentParser(
        description="Beispiel fuer osk-guard in Python")
    parser.add_argument("--stand", default="stand.json",
                        help="Datei des Pruefstands")
    args = parser.parse_args()
    try:
        run(args.stand)
    except osk_guard.OskGuardError as error:
        print("FEHLER: " + error.code, file=sys.stderr)
        return 1
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as error:
        print("FEHLER: %s" % error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
