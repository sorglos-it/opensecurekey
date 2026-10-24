# Pruefungen der Dongle- und Server-Befehle (14 bis 18) am PC. Kein Geraet,
# kein Chip, keine Fremdbibliothek.
#
#   make              - alles uebersetzen und laufen lassen
#   make fuzz         - Fuzzlauf ueber den Rahmen-Parser (Dauer FUZZ_SECS)
#   make CC=clang-19  - mit dem zweiten Uebersetzer
#
# Ein gruener Lauf heisst: uebersetzt und am PC ausgefuehrt. Am Geraet ist
# nichts davon gelaufen - die Platine ist nicht gebaut. Siehe
# docs/dongle-firmware.md.

CC      ?= gcc
GUARD   := ../../src/classes/applets/guard
CTAP    := ../../src/classes/ctap
# test_util.h liegt beim CTAP-Pruefsatz; sie wird geteilt und nicht kopiert.
INC     := -I../../include -I$(GUARD) -I$(CTAP) -I. -I../ctap
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC)
LDFLAGS :=

# Sanitizer, wenn die Werkzeugkette sie hat. Der Rahmen-Parser ist der Teil,
# den ein Angreifer zuerst erreicht - hier wollen wir jeden Ueberlauf sehen.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

CORE := $(GUARD)/osk_guard_msg.c $(GUARD)/osk_guard_grant.c $(GUARD)/osk_guard_keys.c \
        $(GUARD)/osk_guard_fw.c $(CTAP)/osk_cbor.c $(CTAP)/osk_crypto.c
MOCKS := mocks.c

TESTS := test_msg test_guard test_node test_forgery
FUZZERS := fuzz_frame
FUZZ_SECS ?= 20

BUILD := build

all: run

$(BUILD):
	mkdir -p $(BUILD)

# Die Bytefolgen brauchen weder Attrappen noch den Rest.
$(BUILD)/test_msg: test_msg.c $(GUARD)/osk_guard_msg.c | $(BUILD)
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
