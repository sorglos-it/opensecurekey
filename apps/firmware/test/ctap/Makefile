# Pruefungen des CTAP-Kerns am PC. Kein Geraet, keine Fremdbibliothek.
#
#   make        - alles uebersetzen und laufen lassen
#   make fuzz   - Fuzzlauf ueber CBOR- und HID-Parser (Dauer FUZZ_SECS)
#
# Ein gruener Lauf heisst: uebersetzt und ausgefuehrt. Am Geraet ist nichts
# davon gelaufen - siehe docs/ctap.md.
#
# Auf Rechnern ohne gcc und ohne make gibt es denselben Lauf als build.ps1
# (Visual Studio Build Tools). Die Quellenliste steht damit zweimal, und
# `build.ps1 -Vergleich` prueft die beiden gegeneinander statt es zu glauben -
# bei test/se050 fehlte in der einen Beschreibung eine Quelle, die die andere
# fuehrte, und der Windows-Weg band deshalb nie.

# RSA=1 (Vorgabe) setzt CONFIG_OSK_RSA. Die Option entscheidet in
# osk_ctap_core.c, welcher der beiden AAGUIDs gilt - ein AAGUID bezeichnet
# nach FIDO eine Fassung mit denselben Faehigkeiten, darum zwei.
#
# BEIDE FASSUNGEN muessen gebaut und geprueft werden: eine Option, die nur
# in einer Richtung geprueft ist, ist keine (docs/plan.md, Abschnitt 8).
#   make RSA=0    - die Fassung OHNE RSA
#
# Ohne diese Option brach der Bau am 30.10.2026 ab: ohne CONFIG_OSK_RSA
# nimmt osk_ctap_aaguid() den #else-Zweig, OSK_AAGUID_RSA bleibt unbenutzt,
# und -Werror=unused-const-variable macht daraus einen Fehler. Der Prueflauf
# war damit rot, nicht weil der Code falsch war, sondern weil nur eine
# Richtung der Option gebaut wurde.
RSA ?= 1
ifeq ($(RSA),1)
CFLAGS_RSA := -DCONFIG_OSK_RSA=1
endif

CC      ?= gcc
SRC     := ../../src/classes/ctap
TRANS   := ../../src/classes/transport
INC     := -I../../include -I$(SRC) -I$(TRANS) -I.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC) -DOSK_CTAP_TEST_CLOCK $(CFLAGS_RSA)
LDFLAGS :=

# Sanitizer, wenn die Werkzeugkette sie hat. Der Parser ist der
# gefaehrlichste Teil - hier wollen wir jeden Ueberlauf sehen.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

CORE := $(SRC)/osk_cbor.c $(SRC)/osk_crypto.c $(SRC)/osk_credid.c \
        $(SRC)/osk_ctap_key.c $(SRC)/osk_ctap_ext.c $(SRC)/osk_ctap_core.c \
        $(SRC)/osk_ctap_u2f.c $(SRC)/osk_pin.c \
        $(SRC)/osk_ctap_cm.c $(SRC)/osk_ctap_config.c $(SRC)/osk_sigcount.c \
        $(TRANS)/usb_hid.c ../../src/osk_mem.c
MOCKS := mocks.c

TESTS := test_crypto test_cbor test_credid test_pin test_ctap test_u2f test_hid \
         test_sigcount test_credmgmt test_config
FUZZERS := fuzz_cbor fuzz_hid
FUZZ_SECS ?= 20

BUILD := build

all: run

$(BUILD):
	mkdir -p $(BUILD)

# test_crypto und test_cbor brauchen die Attrappen nicht.
$(BUILD)/test_crypto: test_crypto.c $(SRC)/osk_crypto.c ../../src/osk_mem.c | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_cbor: test_cbor.c $(SRC)/osk_cbor.c | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_%: test_%.c $(CORE) $(MOCKS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/fuzz_%: fuzz_%.c $(CORE) $(MOCKS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

run: $(addprefix $(BUILD)/,$(TESTS))
	@fail=0; for t in $^; do echo "== $$t"; $$t || fail=1; done; \
	 if [ $$fail -eq 0 ]; then echo "alle pruefungen: ok"; else echo "PRUEFUNG ROT"; fi; \
	 exit $$fail

fuzz: $(addprefix $(BUILD)/,$(FUZZERS))
	@fail=0; for f in $^; do echo "== $$f"; $$f $(FUZZ_SECS) || fail=1; done; \
	 if [ $$fail -eq 0 ]; then echo "fuzzlauf: ok"; else echo "FUZZLAUF ROT"; fi; \
	 exit $$fail

clean:
	rm -rf $(BUILD)

.PHONY: all run fuzz clean
