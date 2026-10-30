# Pruefungen des PQC-Moduls am PC. Kein Geraet.
#
#   make          - alles uebersetzen und laufen lassen
#   make SAN=     - ohne Sanitizer (MinGW kann sie nicht binden)
#   make budget   - Flash- und Stapelbedarf messen (siehe unten)
#
# Ein gruener Lauf heisst: uebersetzt und ausgefuehrt, am PC. Am Geraet
# ist nichts davon gelaufen - siehe docs/pqc.md.
#
# Keine Fremdbibliothek. Keccak, das Rechenwerk und ML-KEM sind eigener
# Code; die amtlichen Testwerte stehen in kat_mlkem768.h.

CC      ?= gcc
SRC     := ../../src/classes/pqc
INC     := -I../../include -I$(SRC) -I.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC)

# Sanitizer, wenn die Werkzeugkette sie hat. Dieses Modul nimmt Puffer
# vom Draht entgegen (Schluesseltext, fremder oeffentlicher Schluessel) -
# hier wollen wir jeden Ueberlauf sehen.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

MODULE  := $(SRC)/pqc_keccak.c $(SRC)/pqc_poly.c $(SRC)/pqc_mlkem.c $(SRC)/pqc_hybrid.c \
           ../../src/osk_mem.c
TESTS   := test_keccak.c test_poly.c test_mlkem.c test_hybrid.c test_main.c

BUILD := build

all: run

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/pqc_tests: $(TESTS) $(MODULE) kat_mlkem768.h tiny_test.h | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $(TESTS) $(MODULE)

# Die Zeile "pqc: N Faelle, M Fehler" liest tools/check.py aus. Ohne sie
# zaehlt der Lauf dort als "keine Pruefung ausgefuehrt" - und ein gruener
# Code ohne eine einzige Pruefung ist kein gruener Lauf.
run: $(BUILD)/pqc_tests
	@$(BUILD)/pqc_tests

# --- Messen statt schaetzen ---------------------------------------------
#
# Der Plan rechnet mit etwa 11 kB Flash. Dieses Ziel misst, was wirklich
# dabei herauskommt: -Os ohne Sanitizer, ohne Pruefcode, und zusaetzlich
# den Stapelbedarf je Funktion (-fstack-usage).
#
# Zwei Dinge dazu ehrlich gesagt: gemessen wird mit dem Uebersetzer
# dieses Rechners. Ist das kein arm-none-eabi-gcc, ist die Zahl ein
# Anhaltspunkt und keine Zahl fuer den nRF52840. Und: der Stapelbedarf
# ist die Summe entlang des tiefsten Wegs, nicht die Summe aller Zeilen.
budget: | $(BUILD)
	@mkdir -p $(BUILD)/budget
	@for f in $(SRC)/pqc_keccak.c $(SRC)/pqc_poly.c $(SRC)/pqc_mlkem.c $(SRC)/pqc_hybrid.c; do \
		$(CC) -std=c11 -Os -fstack-usage $(INC) -c $$f -o $(BUILD)/budget/`basename $$f .c`.o; \
		mv `basename $$f .c`.su $(BUILD)/budget/ 2>/dev/null || true; \
	done
	@echo ''
	@echo 'Flash (.text + .rodata je Datei, Uebersetzer dieses Rechners):'
	@size -A $(BUILD)/budget/*.o | awk '/^\.text|^\.rodata|^===|o:/ {print}'
	@echo ''
	@echo 'Stapel je Funktion (Byte), groesste zuerst:'
	@cat $(BUILD)/budget/*.su | awk -F'\t' '{print $$2, $$1}' | sort -rn | head -20

clean:
	rm -rf $(BUILD)

.PHONY: all run budget clean
