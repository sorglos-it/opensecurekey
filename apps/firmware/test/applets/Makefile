# Pruefungen der Kartenanwendungen am PC. Kein Geraet, kein Kartenleser,
# keine Fremdbibliothek.
#
#   make            - alles uebersetzen und laufen lassen
#   make fuzz       - Fuzzlauf ueber APDU- und CCID-Parser (Dauer FUZZ_SECS)
#   make CC=clang-19 - mit dem zweiten Uebersetzer
#
# Ein gruener Lauf heisst: uebersetzt und am PC ausgefuehrt. Am Geraet ist
# nichts davon gelaufen, und mit einem echten Kartenleser-Stapel (pcscd,
# Windows, macOS) auch nicht - siehe docs/smartcard.md.

CC      ?= gcc
APP     := ../../src/classes/applets
TRANS   := ../../src/classes/transport
# test_util.h liegt beim CTAP-Pruefsatz; sie wird geteilt und nicht kopiert.
INC     := -I../../include -I$(APP) -I$(TRANS) -I. -I../ctap
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC)
LDFLAGS :=

# Sanitizer, wenn die Werkzeugkette sie hat. Der APDU-Parser ist der Teil,
# den ein Angreifer zuerst erreicht - hier wollen wir jeden Ueberlauf sehen.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

CORE := $(APP)/osk_apdu.c $(APP)/osk_applets.c $(APP)/osk_card.c $(APP)/osk_piv.c \
        $(APP)/osk_openpgp.c $(APP)/osk_otp.c $(APP)/osk_otp_crypto.c \
        $(TRANS)/usb_ccid.c
MOCKS := mocks.c

TESTS := test_crypto test_apdu test_ccid test_piv test_openpgp test_otp
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

clean:
	rm -rf $(BUILD)

.PHONY: all run fuzz clean
