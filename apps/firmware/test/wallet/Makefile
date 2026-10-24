# Pruefungen der Geldboerse am PC. Kein Geraet, keine Fremdbibliothek.
#
#   make        - alles uebersetzen und laufen lassen
#   make fuzz   - Fuzzlauf ueber PSBT- und RLP-Parser (Dauer FUZZ_SECS)
#
# Ein gruener Lauf heisst: uebersetzt und ausgefuehrt. Am Geraet ist nichts
# davon gelaufen - siehe docs/wallet.md.

CC      ?= gcc
SRC     := ../../src/classes/wallet
CTAP    := ../../src/classes/ctap
INC     := -I../../include -I$(SRC) -I$(CTAP) -I../ctap -I.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC)
LDFLAGS :=

# Sanitizer, wenn die Werkzeugkette sie hat. Geld ist der Bereich, in dem ein
# Ueberlauf nicht zu reparieren ist - hier wollen wir jeden sehen.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

# Alle Quellen der Geldboerse, ueber ein Muster: so kann keine neue Datei
# still unter den Tisch fallen. Genau das ist dem CTAP-Pruefsatz einmal
# passiert, als eine Datei dazukam und in keiner Liste stand.
CORE := $(wildcard $(SRC)/*.c) $(CTAP)/osk_crypto.c

MOCKS := mocks.c

TESTS := test_hash test_bn test_ec test_codec test_bip39 test_bip32 test_btc \
         test_psbt test_rlp test_eth test_confirm test_apdu
FUZZERS := fuzz_psbt fuzz_rlp
FUZZ_SECS ?= 20

BUILD := build

all: run

$(BUILD):
	mkdir -p $(BUILD)

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
