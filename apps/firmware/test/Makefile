# Sammel-Baubeschreibung ueber ALLE Pruefsaetze der Firmware.
#
#   make            - jeden Satz, beide Richtungen der RSA-Option
#   make rsa        - nur die Fassung MIT RSA
#   make norsa      - nur die Fassung OHNE RSA
#   make clean      - alle Baureste aller Saetze wegraeumen
#
# Diese Datei traegt keine Quelldateien und keine Uebersetzerschalter: sie
# ruft alle.sh auf, und alle.sh ruft die Baubeschreibung jedes Ordners auf.
# Jede Zahl steht damit an genau einer Stelle. Die Saetze werden von alle.sh
# GEFUNDEN, nicht hier aufgezaehlt - ein neuer Ordner mit einer
# Baubeschreibung wird ab dem ersten Lauf mitgemessen. Genau das Nachtragen
# ist am 29./30.10.2026 vergessen worden und hat 19 Stunden rote Pruefsaetze
# gekostet.
#
# Der Lauf endet mit einem Ausstiegscode ungleich 0, sobald irgendein Satz
# rot ist, und nennt am Ende Gesamtzahl und Gesamtfehler.

.PHONY: all rsa norsa clean

all:
	./alle.sh

rsa:
	./alle.sh RSA=1

norsa:
	./alle.sh RSA=0

clean:
	@for d in */; do \
		d=$${d%/}; \
		[ -f "$$d/Makefile" ] && $(MAKE) -C "$$d" clean >/dev/null 2>&1; \
		rm -rf "$$d/build" "$$d"/build-rsa*; \
	done; true
