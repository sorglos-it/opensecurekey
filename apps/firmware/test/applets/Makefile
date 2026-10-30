# Pruefungen der Kartenanwendungen am PC. Kein Geraet, kein Kartenleser,
# keine Fremdbibliothek.
#
#   make            - alles uebersetzen und laufen lassen
#   make fuzz       - Fuzzlauf ueber APDU- und CCID-Parser (Dauer FUZZ_SECS)
#   make CC=clang-19 - mit dem zweiten Uebersetzer
#   make RSA=0      - die Fassung OHNE RSA (Kconfig-Option CONFIG_OSK_RSA)
#
# BEIDE FASSUNGEN muessen gebaut und geprueft werden: eine Option, die nur
# in einer Richtung geprueft ist, ist keine (docs/plan.md, Abschnitt 8).
# Unter Windows gibt es weder make noch gcc; dort ruft run.ps1 dieses
# Makefile in der WSL auf. Es gibt absichtlich nur DIESE eine
# Baubeschreibung - zwei, die sich widersprechen, haben den Prueflauf des
# SE050-Treibers am 28.10.2026 schon einmal still gebrochen.
#
# Ein gruener Lauf heisst: uebersetzt und am PC ausgefuehrt. Am Geraet ist
# nichts davon gelaufen, und mit einem echten Kartenleser-Stapel (pcscd,
# Windows, macOS) auch nicht - siehe docs/smartcard.md.

CC      ?= gcc
APP     := ../../src/classes/applets
TRANS   := ../../src/classes/transport
# test_util.h liegt beim CTAP-Pruefsatz; sie wird geteilt und nicht kopiert.
# ../ctap liefert zwei Dinge: test_util.h (das Pruefgeruest, geteilt und
# nicht kopiert) und osk_crypto.h mit SHA-256. Letzteres braucht die
# Attestierung; SHA-256 wird nicht zum zweiten Mal geschrieben, genauso
# wie die Geldboerse es haelt (src/classes/wallet/wallet_hash.h).
CTAP    := ../../src/classes/ctap
INC     := -I../../include -I$(APP) -I$(TRANS) -I. -I../ctap -I$(CTAP)
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC)
LDFLAGS :=

# Sanitizer, wenn die Werkzeugkette sie hat. Der APDU-Parser ist der Teil,
# den ein Angreifer zuerst erreicht - hier wollen wir jeden Ueberlauf sehen.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

# RSA=1 (Vorgabe) setzt CONFIG_OSK_RSA. Die Option wirkt an genau einer
# Stelle, osk_rsa_available() - siehe Kopf von osk_rsa_card.h.
RSA ?= 1
ifeq ($(RSA),1)
CFLAGS += -DCONFIG_OSK_RSA=1
endif

CORE := $(APP)/osk_apdu.c $(APP)/osk_applets.c $(APP)/osk_card.c $(APP)/osk_piv.c \
        $(APP)/osk_openpgp.c $(APP)/osk_otp.c $(APP)/osk_otp_crypto.c \
        $(APP)/osk_rsa_card.c $(APP)/osk_serial.c $(APP)/osk_static_pw.c \
        $(APP)/osk_attest.c $(APP)/osk_der.c $(APP)/osk_spki.c $(APP)/osk_csr.c \
        $(CTAP)/osk_crypto.c ../../src/osk_mem.c \
        $(TRANS)/usb_ccid.c
MOCKS := mocks.c

TESTS := test_crypto test_apdu test_ccid test_piv test_openpgp test_otp \
         test_rsa_card test_attest test_csr test_static_pw test_serial
FUZZERS := fuzz_apdu fuzz_ccid
FUZZ_SECS ?= 20

BUILD := build

all: run

$(BUILD):
	mkdir -p $(BUILD)

# Diese zwei brauchen weder Attrappen noch den Rest.
$(BUILD)/test_crypto: test_crypto.c $(APP)/osk_otp_crypto.c | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_apdu: test_apdu.c $(APP)/osk_apdu.c | $(BUILD)
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

# --- Beleg mit einem FREMDEN Programm ----------------------------------
#
# Eine selbstgebaute Zertifikatsanfrage, die nur das eigene Leseprogramm
# versteht, ist nichts wert. `make openssl` baut tool_csr und laesst
# csr_openssl.py openssl darueber laufen: openssl erzeugt die Schluessel,
# unterschreibt den von der Firmware gebauten Teil und bestaetigt
# hinterher die Unterschrift in der fertigen Anfrage.
#
# Das laeuft NICHT bei `make` mit: es braucht openssl und python3 daneben
# und schreibt Dateien. Ein Pruefsatz, der ohne Werkzeuge nicht laeuft,
# wuerde sonst still uebersprungen - und ein uebersprungener Beleg sieht
# aus wie ein erbrachter.
$(BUILD)/tool_csr: tool_csr.c $(CORE) $(MOCKS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

openssl: $(BUILD)/tool_csr
	python3 csr_openssl.py --tool $(BUILD)/tool_csr --out $(BUILD)/csr

clean:
	rm -rf $(BUILD)

.PHONY: all run fuzz openssl clean
