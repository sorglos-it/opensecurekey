#!/bin/sh
# Flash- und Stapelbedarf des PQC-Moduls fuer den nRF52840 messen.
#
#   sh budget.sh            - mit arm-none-eabi-gcc, wenn vorhanden
#   CC=gcc sh budget.sh     - notfalls mit dem Uebersetzer des Rechners
#
# Gemessen wird mit -Os, ohne Pruefcode und ohne Sanitizer: so wird das
# Modul im Abbild uebersetzt. Die Zahl unter "Stapel" ist die Summe
# entlang des tiefsten Wegs, nicht die Summe aller Zeilen - Entkapseln
# ruft Entschluesseln und Verschluesseln NACH einander, nicht
# ineinander, darum zaehlt der groessere der beiden.
#
# Warum ein eigenes Skript und kein Ziel im Makefile: das Makefile baut
# den Prueflauf fuer diesen Rechner, hier wird fuer das Geraet gemessen.
# Zwei verschiedene Fragen, zwei Aufrufe.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
FW=$(cd "$HERE/../.." && pwd)
SRC="$FW/src/classes/pqc"
OUT="$HERE/build/budget"

CC=${CC:-arm-none-eabi-gcc}
SIZE=${SIZE:-$(echo "$CC" | sed 's/gcc$/size/')}
ARCH="-mcpu=cortex-m4 -mthumb"
case "$CC" in
*arm-none-eabi-gcc) ;;
*) ARCH=""; SIZE=size ;;
esac

rm -rf "$OUT"
mkdir -p "$OUT"
cd "$OUT"

for name in pqc_keccak pqc_poly pqc_mlkem pqc_hybrid; do
	$CC $ARCH -std=c11 -Os -ffunction-sections -fdata-sections -fstack-usage \
		-Wall -I"$FW/include" -I"$SRC" -c "$SRC/$name.c" -o "$name.o"
done

echo "Uebersetzer: $CC"
echo ""
echo "Flash und feste Daten (Byte):"
$SIZE -t *.o
echo ""
echo "Stapel je Funktion (Byte), groesste zuerst:"
cat *.su | awk -F'\t' '{ printf "%8s  %-28s %s\n", $2, $1, $3 }' | sort -rn
