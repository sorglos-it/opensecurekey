# Pruefungen des SE050-Treibers am PC. Kein Geraet.
#
#   make        - alles uebersetzen und laufen lassen
#   make SAN=   - ohne Sanitizer (MinGW kann sie nicht binden)
#
# Ein gruener Lauf heisst: uebersetzt und ausgefuehrt. Am Geraet ist nichts
# davon gelaufen - siehe docs/se050.md.
#
# AES und AES-CMAC kommen aus libcrypto (OpenSSL), nicht aus eigenem Code.
# Unter Windows nimmt build.ps1 statt dessen Windows CNG; beide bedienen
# denselben Kopf se050_crypto.h.

CC      ?= gcc
SRC     := ../../src/classes/se050
INC     := -I../../include -I$(SRC) -I.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC)
LDFLAGS := -lcrypto

# Sanitizer, wenn die Werkzeugkette sie hat. Diese Schicht nimmt Puffer vom
# Draht entgegen - hier wollen wir jeden Ueberlauf sehen.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

# se050_crypto_psa.c fehlt absichtlich: das braucht das nRF Connect SDK.
# se050_crypto_cng.c ebenso: das ist der Windows-Weg.
DRIVER := $(SRC)/se050_crc.c $(SRC)/se050_tlv.c $(SRC)/se050_t1.c $(SRC)/se050_scp03.c \
          $(SRC)/se050_apdu.c $(SRC)/se050.c $(SRC)/se050_rsa.c $(SRC)/se050_counter.c $(SRC)/se050_crypto_common.c \
          $(SRC)/se050_crypto_openssl.c $(SRC)/se050_commission.c ../../src/osk_mem.c
HARNESS := fake_bus.c se050_sim.c test_platform.c
TESTS   := test_crc.c test_tlv.c test_t1.c test_scp03.c test_errors.c test_contract.c \
           test_rsa.c test_counter.c test_commission.c test_main.c

BUILD := build

all: run

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/se050_tests: $(TESTS) $(HARNESS) $(DRIVER) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

# Die Zeile "se050: N Faelle, M Fehler" liest tools/check.py aus. Ohne sie
# zaehlt der Lauf dort als "keine Pruefung ausgefuehrt" - und ein gruener
# Code ohne eine einzige Pruefung ist kein gruener Lauf.
run: $(BUILD)/se050_tests
	@$(BUILD)/se050_tests

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
