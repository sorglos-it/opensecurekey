#!/bin/sh
# Der FREMDE Beleg: openssl rechnet nach, was dieser Code gerechnet hat.
#
# Warum das noetig ist: der eigene Pruefsatz rechnet mit demselben Code
# nach, der die Zahlen gebildet hat. Das geht auch mit einer falschen
# Umsetzung auf. openssl weiss nichts von diesem Projekt - stimmt es
# zu, ist das ein Beleg von aussen.
#
# Was hier geprueft wird:
#   1. Der oeffentliche Schluessel. openssl rechnet ihn aus dem privaten
#      selbst aus; er muss auf das Byte mit unserem uebereinstimmen.
#      Damit ist die Punktmultiplikation auf der Kurve von aussen belegt.
#   2. Die Signatur. openssl prueft sie gegen den oeffentlichen
#      Schluessel und den Hash. Damit ist ECDSA von aussen belegt.
#   3. Die Gegenprobe: ein verbogener Hash muss DURCHFALLEN. Ohne sie
#      wuesste niemand, ob openssl ueberhaupt richtig hinsieht - eine
#      Pruefung, die immer ja sagt, ist keine.
#
# Was hier NICHT geprueft wird: die Schluesselableitung (dafuer liegen
# die amtlichen BIP32-Vektoren im Pruefsatz) und das Bilden der
# Transaktionshashes (dafuer die Beispiele aus EIP-155 und EIP-712).
#
# Aufruf: sh fremdbeleg.sh
set -u

if ! command -v openssl >/dev/null 2>&1; then
	echo "openssl fehlt - der fremde Beleg kann nicht erbracht werden."
	echo "Ohne ihn gilt: der eigene Pruefsatz rechnet mit dem eigenen Code nach."
	exit 2
fi

echo "openssl: $(openssl version)"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

if ! ./build/fremdbeleg > "$WORK/zahlen"; then
	echo "FEHLER: das Hilfsprogramm lief nicht"
	exit 1
fi
. "$WORK/zahlen"

for v in PRIV PUB_COMPRESSED PUB_UNCOMPRESSED HASH SIG_DER; do
	eval "val=\$$v"
	if [ -z "$val" ]; then
		echo "FEHLER: $v fehlt"
		exit 1
	fi
done

# Hex nach Bytes. Nicht mit xxd: das liegt nicht auf jedem System, und
# ein Beleg, der an einem fehlenden Werkzeug scheitert, ist keiner.
hex2bin() {
	python3 -c 'import sys,binascii;sys.stdout.buffer.write(binascii.unhexlify(sys.argv[1]))' "$1"
}
bin2hex() { od -An -tx1 | tr -d ' 
'; }

# --- 1. Der oeffentliche Schluessel ------------------------------------
#
# Ein ECPrivateKey nach SEC1, von Hand aufgebaut: SEQUENCE { INTEGER 1,
# OCTET STRING(32) priv, [0] { OID secp256k1 } }. Der oeffentliche Teil
# fehlt absichtlich - openssl soll ihn SELBST ausrechnen.
hex2bin "302e0201010420${PRIV}a00706052b8104000a" > "$WORK/priv.der"
if ! openssl ec -inform DER -in "$WORK/priv.der" -pubout -outform DER \
	-out "$WORK/pub.der" 2>"$WORK/err"; then
	echo "FEHLER: openssl konnte den privaten Schluessel nicht lesen"
	cat "$WORK/err"
	exit 1
fi

# Aus dem SubjectPublicKeyInfo den Punkt herausholen: die letzten 65 Byte.
OSSL_PUB=$(bin2hex < "$WORK/pub.der" | tail -c 130)

if [ "$OSSL_PUB" = "$PUB_UNCOMPRESSED" ]; then
	echo "ok   1. oeffentlicher Schluessel: openssl rechnet denselben Punkt aus"
else
	echo "FEHLER 1. oeffentlicher Schluessel weicht ab"
	echo "  wir:     $PUB_UNCOMPRESSED"
	echo "  openssl: $OSSL_PUB"
	exit 1
fi

# --- 2. Die Signatur ---------------------------------------------------
hex2bin "$HASH" > "$WORK/hash.bin"
hex2bin "$SIG_DER" > "$WORK/sig.der"

if openssl pkeyutl -verify -pubin -keyform DER -inkey "$WORK/pub.der" \
	-in "$WORK/hash.bin" -sigfile "$WORK/sig.der" >/dev/null 2>"$WORK/err"; then
	echo "ok   2. Signatur: openssl bestaetigt sie"
else
	echo "FEHLER 2. openssl verwirft unsere Signatur"
	cat "$WORK/err"
	exit 1
fi

# --- 3. Die Gegenprobe -------------------------------------------------
#
# Ein einzelnes verbogenes Bit im Hash muss openssl zum Nein bringen.
# Schluckt es auch das, prueft der ganze Lauf nichts.
BAD=$(printf '%s' "$HASH" | sed 's/^../00/')
if [ "$BAD" = "$HASH" ]; then
	BAD=$(printf '%s' "$HASH" | sed 's/^../11/')
fi
hex2bin "$BAD" > "$WORK/bad.bin"

if openssl pkeyutl -verify -pubin -keyform DER -inkey "$WORK/pub.der" \
	-in "$WORK/bad.bin" -sigfile "$WORK/sig.der" >/dev/null 2>&1; then
	echo "FEHLER 3. openssl nimmt auch einen falschen Hash an - die Pruefung prueft nichts"
	exit 1
else
	echo "ok   3. Gegenprobe: openssl verwirft den verbogenen Hash"
fi

echo
echo "fremder Beleg: erbracht (openssl $(openssl version | cut -d' ' -f2))"
