# Pruefungen der Rechteschicht am PC. Kein Geraet.
#
#   make        - alles uebersetzen und laufen lassen
#   make SAN=   - ohne Sanitizer (MinGW kann sie nicht binden)
#
# Ein gruener Lauf heisst: uebersetzt und ausgefuehrt. Am Geraet ist nichts
# davon gelaufen - siehe docs/rechtebereiche.md.
#
# Keine Fremdbibliothek: SHA-256 und HMAC kommen aus
# src/classes/ctap/osk_crypto.c. Unter Windows nimmt build.ps1 dieselben
# Quellen in derselben Reihenfolge - die beiden Listen gehoeren verglichen,
# wenn eine Datei dazukommt.

CC      ?= gcc
SRC     := ../../src/classes/authz
CTAP    := ../../src/classes/ctap
INC     := -I../../include -I$(SRC) -I$(CTAP) -I../se050 -I.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC)

# Sanitizer, wenn die Werkzeugkette sie hat. Der DER-Leser nimmt Puffer vom
# Draht entgegen - hier wollen wir jeden Ueberlauf sehen.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

LAYER   := $(SRC)/authz_der.c $(SRC)/authz_core.c $(SRC)/authz_keys.c \
           $(SRC)/authz_cred.c $(SRC)/authz_tsa.c \
           $(CTAP)/osk_crypto.c ../../src/osk_mem.c
HARNESS := fake_se050.c
TESTS   := test_der.c test_realm.c test_keys.c test_cred.c test_tsa.c test_main.c

BUILD := build

all: run

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/authz_tests: $(TESTS) $(HARNESS) $(LAYER) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^

# Die Zeile "authz: N Faelle, M Fehler" liest tools/check.py aus. Ohne sie
# zaehlt der Lauf dort als "keine Pruefung ausgefuehrt" - und ein gruener
# Code ohne eine einzige Pruefung ist kein gruener Lauf.
run: $(BUILD)/authz_tests
	@$(BUILD)/authz_tests

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
