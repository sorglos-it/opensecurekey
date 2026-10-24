# Pruefungen des Laufwerks am PC. Kein Geraet, keine Fremdbibliothek.
#
#   make        - alles uebersetzen und laufen lassen
#   make fuzz   - Fuzzlauf ueber den SCSI-Parser
#
# Ein gruener Lauf heisst: uebersetzt und ausgefuehrt, gegen eine Attrappe im
# RAM. **Kein Rechner hat dieses Laufwerk je gesehen** - es gibt noch keine
# Platine. Siehe docs/laufwerk.md.

CC      ?= gcc
SRC     := ../../src/classes/transport
INC     := -I../../include -I$(SRC) -I../ctap -I.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC)
LDFLAGS :=

# Sanitizer, wenn die Werkzeugkette sie hat. Der SCSI-Parser steht an einer
# Schnittstelle, die jeder Rechner anspricht, ohne dass jemand zustimmt -
# hier zaehlt jeder Ueberlauf, den kein Fall sieht.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

MSC     := $(SRC)/usb_msc.c
MOCKS   := mocks.c

TESTS   := test_msc test_readonly
FUZZERS := fuzz_scsi
FUZZ_SECS ?= 20

BUILD := build

all: run

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_msc: test_msc.c $(MSC) $(MOCKS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_readonly: test_readonly.c $(MSC) $(MOCKS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/fuzz_scsi: fuzz_scsi.c $(MSC) $(MOCKS) | $(BUILD)
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
