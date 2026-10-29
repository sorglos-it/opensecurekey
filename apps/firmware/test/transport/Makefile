# Pruefungen des Funkteils am PC. Kein Geraet, keine Fremdbibliothek.
#
#   make        - alles uebersetzen und laufen lassen
#   make fuzz   - Fuzzlauf ueber den ISO-DEP- und den NDEF-Parser
#
# Ein gruener Lauf heisst: uebersetzt und ausgefuehrt. Es ist KEINE
# Funkmessung dabei - es gibt noch keine Platine. Siehe docs/funk.md.

CC      ?= gcc
SRC     := ../../src/classes/transport
CTAP    := ../../src/classes/ctap
INC     := -I../../include -I$(SRC) -I$(CTAP) -I../ctap -I.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC) -DOSK_CTAP_TEST_CLOCK
LDFLAGS :=

# Sanitizer, wenn die Werkzeugkette sie hat. Die Parser sind der
# gefaehrlichste Teil - ein Angreifer mit einer Antenne steht naeher am
# Geraet als einer am USB.
SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

NFC   := $(SRC)/nfc_apdu.c $(SRC)/nfc_isodep.c $(SRC)/nfc_ctap.c $(SRC)/nfc_ndef.c \
         $(SRC)/nfc_tag.c
BLE   := $(SRC)/ble_pair.c $(SRC)/ble_gatt.c
# Die Attrappen des CTAP-Strangs: Anzeige, Uhr, SE050, Tresor. Sie werden
# hier wiederverwendet statt nachgebaut - ein zweiter Satz waere ein
# zweiter Ort, an dem dieselbe Luege stehen koennte.
MOCKS := ../ctap/mocks.c
# Die Attrappen rechnen mit den echten Hashfunktionen des Kerns.
CRYPTO := $(CTAP)/osk_crypto.c ../../src/osk_mem.c
# Der echte CTAP-Kern. Nur der Satz test_nfc nimmt ihn, damit dort wirklich
# derselbe Kern antwortet wie am USB - und der Tastendruck echt gezaehlt wird.
CORE  := $(CTAP)/osk_cbor.c $(CTAP)/osk_crypto.c $(CTAP)/osk_credid.c \
         $(CTAP)/osk_ctap_key.c $(CTAP)/osk_ctap_ext.c $(CTAP)/osk_ctap_core.c \
         $(CTAP)/osk_ctap_u2f.c $(CTAP)/osk_pin.c ../../src/osk_mem.c

TESTS   := test_apdu test_isodep test_ndef test_ble test_nfc_chain test_nfc
FUZZERS := fuzz_isodep fuzz_ndef
FUZZ_SECS ?= 20

BUILD := build

all: run

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_apdu: test_apdu.c $(SRC)/nfc_apdu.c | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_isodep: test_isodep.c $(SRC)/nfc_isodep.c $(MOCKS) $(CRYPTO) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_ndef: test_ndef.c $(SRC)/nfc_ndef.c $(SRC)/nfc_apdu.c $(MOCKS) $(CRYPTO) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_ble: test_ble.c $(BLE) $(MOCKS) $(CRYPTO) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

# Ohne den Kern: dieser Satz stellt den Kern selbst, damit auch lange
# Antworten und Fehlerfaelle pruefbar sind.
$(BUILD)/test_nfc_chain: test_nfc_chain.c $(NFC) $(MOCKS) $(CRYPTO) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

# Mit dem echten Kern.
$(BUILD)/test_nfc: test_nfc.c $(NFC) $(SRC)/usb_hid.c $(CORE) $(MOCKS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/fuzz_isodep: fuzz_isodep.c $(NFC) $(MOCKS) $(CRYPTO) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/fuzz_ndef: fuzz_ndef.c $(SRC)/nfc_ndef.c $(SRC)/nfc_apdu.c $(MOCKS) $(CRYPTO) | $(BUILD)
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

# ---------------------------------------------------------------------
# Strang E - Yubico-HID-Protokoll.
#
# Dieser Teil laeuft nur auf einem Rechner mit gcc und make. Auf dem
# Entwicklungsrechner (Windows) gibt es beides nicht; dort fuehrt
# build_yk.ps1 denselben Satz mit den Visual Studio Build Tools.
#
# Die Quellen stehen in sources_yk_lib.txt und sources_yk_test.txt und
# werden von BEIDEN Wegen daraus gelesen - nicht zweimal aufgezaehlt.
# Begruendung in sources_yk_lib.txt.
YK_LIB  := $(shell grep -v '^[[:space:]]*\#' sources_yk_lib.txt | grep -v '^[[:space:]]*$$')
YK_TEST := $(shell grep -v '^[[:space:]]*\#' sources_yk_test.txt | grep -v '^[[:space:]]*$$')

$(BUILD)/yk_tests: $(YK_TEST) $(YK_LIB) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

$(BUILD)/fuzz_ykhid: fuzz_ykhid.c $(YK_LIB) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^ $(LDFLAGS)

# Der Satz des Strangs E allein - mit Adress- und Verhaltenspruefer,
# falls die Werkzeugkette sie hat (siehe SAN oben).
yk: $(BUILD)/yk_tests
	$(BUILD)/yk_tests

fuzz-yk: $(BUILD)/fuzz_ykhid
	$(BUILD)/fuzz_ykhid $(FUZZ_SECS)

.PHONY: yk fuzz-yk
