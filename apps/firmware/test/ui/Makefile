# Pruefungen der Anzeige am PC. Kein Geraet, keine Fremdbibliothek.
#
#   make          - erzeugte Dateien vergleichen, uebersetzen, laufen lassen
#   make wide     - dasselbe fuer das 128x64-Modul vom Teststand
#   make SAN=     - ohne Sanitizer (MinGW kann sie nicht binden)
#
# Ein gruener Lauf heisst: uebersetzt und ausgefuehrt. Auf einem echten
# Display hat nie etwas davon gestanden - siehe docs/display.md.

CC      ?= gcc
PYTHON  ?= python
UI      := ../../src/classes/ui
VIEWS   := ../../src/views
INC     := -I../../include -I$(UI) -I$(VIEWS) -I.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
           -Wpointer-arith -Wcast-qual -Wundef -O1 -g $(INC)
PANEL   ?=

SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

SRC := $(UI)/osk_gfx.c $(UI)/osk_font_data.c $(UI)/osk_text.c $(UI)/osk_lang.c \
       $(UI)/osk_ssd1306.c $(UI)/osk_button.c $(UI)/osk_ui.c $(UI)/osk_pin.c \
       $(VIEWS)/view_layout.c $(VIEWS)/view_confirm.c $(VIEWS)/view_status.c \
       $(VIEWS)/view_pin.c $(VIEWS)/view_value.c
TESTS := fake_panel.c test_gfx.c test_font.c test_text.c test_lang.c test_button.c \
         test_panel.c test_confirm.c test_flow.c test_pin.c test_main.c

BUILD := build

all: generated run

# Die erzeugten Dateien (Schriften, Texttabelle) muessen zu generate.py und zu
# den fuenf Sprachdateien passen. Weicht eine ab, bricht der Lauf hier ab -
# nicht erst, wenn jemand einen falschen Text auf dem Schirm sieht.
#
# check_lang_index.py prueft zusaetzlich, dass jeder Platz in der Texttabelle in
# allen fuenf Sprachen denselben Begriff meint. Ein verschobener Index ist
# still: die Tabelle uebersetzt noch, zeigt aber den falschen Text.
generated:
	@if $(PYTHON) --version >/dev/null 2>&1; then \
	   $(PYTHON) $(UI)/generate.py --check && $(PYTHON) check_lang_index.py; \
	 elif python3 --version >/dev/null 2>&1; then \
	   python3 $(UI)/generate.py --check && python3 check_lang_index.py; \
	 else \
	   echo "UEBERSPRUNGEN: kein Python - erzeugte Dateien NICHT verglichen"; \
	 fi

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/ui_tests: $(SRC) $(TESTS) | $(BUILD)
	$(CC) $(CFLAGS) $(PANEL) $(SAN) -o $@ $^

run: $(BUILD)/ui_tests
	$(BUILD)/ui_tests

# Dasselbe gegen die zweite Groesse - die Groesse ist eine Konstante, und das
# soll bewiesen sein, nicht behauptet.
wide:
	$(MAKE) PANEL=-DOSK_PANEL_128X64 BUILD=build/wide run

both: all wide

clean:
	rm -rf build

.PHONY: all generated run wide both clean
