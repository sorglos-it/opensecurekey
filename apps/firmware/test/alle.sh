#!/bin/sh
# Laesst JEDEN C-Pruefsatz der Firmware laufen - in beiden Richtungen der
# RSA-Option - und endet mit einem Ausstiegscode ungleich 0, sobald
# irgendetwas rot ist.
#
#   ./alle.sh            - alle Saetze, RSA=1 und RSA=0
#   ./alle.sh RSA=1      - nur die Fassung MIT RSA
#   ./alle.sh ctap ui    - nur diese Saetze (beide Richtungen)
#   make -f Makefile     - dasselbe ueber das Sammel-Makefile
#
# WARUM DIESE DATEI UEBERHAUPT:
# Es gibt je Ordner eine Baubeschreibung, und wer seine eigene laufen laesst,
# sieht die des Nachbarn nicht. Am 29./30.10.2026 hat ein Strang drei Dateien
# zum CTAP-Kern gelegt und nur in SEINEM Makefile nachgetragen; die Saetze
# ctap und transport waren danach 19 Stunden rot, und niemandem fiel es auf,
# weil niemand alles messen konnte. Diese Datei ist der eine Befehl, der es
# kann.
#
# WARUM BASH UND NICHT POWERSHELL:
# make, gcc und cmake gibt es nur in der WSL - also muss die Arbeit dort
# geschehen. alle.ps1 ist deshalb ein duenner Mantel, der NUR diese Datei in
# der WSL aufruft. Zwei Baubeschreibungen, die sich widersprechen, haben den
# Prueflauf des SE050-Treibers am 28.10.2026 schon einmal still gebrochen -
# darum steht die Logik genau einmal, und zwar hier.
#
# WAS EIN GRUENER LAUF HEISST:
# uebersetzt und am PC ausgefuehrt. Am Geraet ist nichts davon gelaufen.
# Die Fuzzlaeufe (make fuzz) und die Belege mit fremden Programmen
# (make openssl, pqc/budget.sh) laufen hier NICHT mit: sie brauchen Zeit
# bzw. Werkzeuge daneben, und ein uebersprungener Beleg sieht aus wie ein
# erbrachter.

set -u
cd "$(dirname "$0")" || exit 2

# Saetze werden GEFUNDEN, nicht aufgezaehlt: ein neuer Ordner mit einer
# Baubeschreibung wird ab dem ersten Lauf mitgemessen, ohne dass hier
# jemand etwas nachtragen muss. Genau das Nachtragen ist letztes Mal
# vergessen worden.
alle_saetze() {
	for d in */; do
		d=${d%/}
		if [ -f "$d/Makefile" ] || [ -f "$d/CMakeLists.txt" ]; then
			printf '%s\n' "$d"
		fi
	done
}

# Das Ziel je Satz. Leer heisst: das vorgegebene Ziel der Baubeschreibung.
# ui bekommt "both": "all" laesst die zweite Schirmgroesse unbeachtet, und
# eine Groesse, die nur in einer Richtung geprueft ist, ist keine.
ziel_von() {
	case "$1" in
	ui) echo both ;;
	*)  echo "" ;;
	esac
}

richtungen="1 0"
saetze=""
for arg in "$@"; do
	case "$arg" in
	RSA=0) richtungen="0" ;;
	RSA=1) richtungen="1" ;;
	-*)    echo "alle.sh: unbekannt: $arg" >&2; exit 2 ;;
	*)     saetze="$saetze $arg" ;;
	esac
done
[ -n "$saetze" ] || saetze=$(alle_saetze)

log=$(mktemp)
trap 'rm -f "$log"' EXIT

sum_faelle=0
sum_fehler=0
rote=""
breite=0
for s in $saetze; do
	[ ${#s} -gt $breite ] && breite=${#s}
done

echo "== Pruefsaetze der Firmware, beide Richtungen der RSA-Option =="
echo

for s in $saetze; do
	if [ ! -d "$s" ]; then
		echo "alle.sh: kein Ordner: $s" >&2
		rote="$rote $s"
		continue
	fi
	for r in $richtungen; do
		if [ -f "$s/Makefile" ]; then
			# Zwischen den Richtungen MUSS geraeumt werden: make sieht
			# nur geaenderte Dateien, nicht geaenderte Uebersetzerschalter.
			# Ohne clean messe man zweimal dieselbe Fassung und halte das
			# fuer zwei Richtungen.
			make -C "$s" clean >/dev/null 2>&1
			make -C "$s" "RSA=$r" $(ziel_von "$s") >"$log" 2>&1
			code=$?
			make -C "$s" clean >/dev/null 2>&1
		elif [ -f "$s/CMakeLists.txt" ]; then
			b="$s/build-rsa$r"
			rm -rf "$b"
			flags=""
			[ "$r" = 1 ] && flags="-DCONFIG_OSK_RSA=1"
			{
				cmake -S "$s" -B "$b" -DCMAKE_BUILD_TYPE=Debug \
					-DCMAKE_C_FLAGS="$flags" >/dev/null &&
				cmake --build "$b" -j >/dev/null &&
				for exe in "$b"/*_tests; do
					[ -x "$exe" ] && "$exe"
				done
			} >"$log" 2>&1
			code=$?
			rm -rf "$b"
		else
			continue
		fi

		# Fallzahl und Fehlerzahl stehen in Zeilen der Form
		#   name: 12 Faelle, 0 Fehler      (applets, ctap, se050, guard, ...)
		#   793 Pruefungen, 0 Fehler       (boot, store, ui)
		# BEIDE Woerter muessen gelesen werden. Der erste Entwurf dieser
		# Datei kannte nur "Faelle"; boot, store und ui meldeten daraufhin
		# "0 Faelle, Code 0, gruen", obwohl sie 793, 1221 bzw. 4656 Faelle
		# wirklich gelaufen sind. Ein uebersprungener Beleg sieht aus wie
		# ein erbrachter - darum ist 0 Faelle weiter unten auch ROT.
		# grep -oE und nicht sed: bei sed musste vor der Zahl noch ein
		# Zeichen stehen ("name: 12 Faelle"), und die Zeile "793
		# Pruefungen, 0 Fehler" faengt mit der Zahl an - die wurde
		# stillschweigend nicht gelesen.
		faelle=$(grep -oE '[0-9]+ (Faelle|Pruefungen)' "$log" |
			awk '{t+=$1} END {print t+0}')
		fehler=$(grep -oE '[0-9]+ Fehler' "$log" |
			awk '{t+=$1} END {print t+0}')
		sum_faelle=$((sum_faelle + faelle))
		sum_fehler=$((sum_fehler + fehler))

		# Null Faelle ist ROT, nicht gruen: ein Satz, der keine Fallzahl
		# gemeldet hat, ist entweder nicht gelaufen oder wird hier nicht
		# verstanden. In beiden Faellen ist nichts bewiesen.
		if [ "$code" -eq 0 ] && [ "$fehler" -eq 0 ] && [ "$faelle" -gt 0 ]; then
			urteil=gruen
		else
			urteil=ROT
			rote="$rote $s(RSA=$r)"
		fi
		printf '%-*s RSA=%s  %5s Faelle  %4s Fehler  Code %-3s %s\n' \
			"$breite" "$s" "$r" "$faelle" "$fehler" "$code" "$urteil"
		# Bei Rot die Ausgabe zeigen - ein roter Satz ohne Grund ist
		# niemandem zu erklaeren.
		if [ "$urteil" = ROT ]; then
			sed 's/^/    | /' "$log" | tail -40
		fi
	done
done

echo
echo "GESAMT: $sum_faelle Faelle, $sum_fehler Fehler"
if [ -n "$rote" ]; then
	echo "ROT:$rote"
	exit 1
fi
echo "alle pruefsaetze: gruen"
exit 0
