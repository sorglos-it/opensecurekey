# Display and on-device approval

Status: written, compiled, tested on a PC. **Nothing has ever been shown on a
real display** — the panel is ordered, not here. Every number below either
comes from a measurement named next to it, or is marked as unverified.

The display exists so the owner does not have to believe the computer. On a
crypto transfer it is the difference between "I sign what the computer tells
me" and "I see the recipient". That is why the one rule of this module is:
**a name, a recipient or an amount is never silently cut off.**

## What is here

| Part | File | Job |
| --- | --- | --- |
| Frame buffer, drawing | `src/classes/ui/osk_gfx.c` | pixels, lines, rectangles, glyphs |
| Fonts | `src/classes/ui/osk_font_data.c` (generated) | two sizes, all five languages |
| Text | `src/classes/ui/osk_text.c` | UTF-8, word wrap, pages |
| Texts | `src/classes/ui/osk_lang.c` + `lang/*.json` | five languages, English fallback |
| Panel driver | `src/classes/ui/osk_ssd1306.c` | start sequence, page writes, column offset |
| Button | `src/classes/ui/osk_button.c` | debounce, short / long / double |
| Approval | `src/classes/ui/osk_ui.c` | `osk_ui_confirm`, `osk_ui_verify_user`, `osk_ui_status` |
| Screens | `src/views/view_*.c` | layout only, no logic |
| Board glue | `src/classes/ui/osk_ui_port_nrf.c` | SPI and GPIO on the nRF52840 — **never compiled** |

The driver knows no bus. It gets four handles (command, data, reset, power),
so the same code runs on the device and on a PC against a dummy that records
the bytes. `osk_ui_port.h` is the contract downwards, `include/osk_ui.h` the
contract the applets already use.

## Two panel sizes, one constant

| Build | Panel | Size | Column offset |
| --- | --- | --- | --- |
| default | CFAL4864A-071BW (0.71", target part) | 48 × 64 | 32 |
| `-DOSK_PANEL_128X64` | 0.96" module (test bench) | 128 × 64 | 0 |

The size is a constant in `osk_gfx.h`, not an assumption spread through the
code. The 48-column part keeps its pixels in the middle of the controller's
128-column memory; without the offset everything would sit 32 columns off.
Both sizes are built and run in the test suite (`make`, `make wide`).

## Layout

On 48 × 64 pixels the layout is the actual work:

```
 ┌────────────────┐  big font, 8 characters per line
 │   Signatur     │  title: what is being approved
 ├────────────────┤
 │Empfaenger:     │  small font, 12 characters per line
 │0x71C7656EC7    │  the recipient, verbatim
 │ab88b098defB    │
 │751B7401B5f6    │
 │d8976F          │
 │Kurz: weiter 1/2│  footer: what the button does, which page
 └────────────────┘
```

A 42-character Ethereum address needs four lines, so it runs over two pages.
Pages are not an afterthought: wrapping is done one line at a time from a byte
offset, so the next page simply continues where the last one stopped. Nothing
is dropped, and the test suite proves it twice — once at the text layer (every
non-space character comes back out) and once from the rendered image (every
wrapped line is found again in the pixels on some page).

A word longer than one line is split hard. That is ugly and deliberate: an
address must be readable in full, and three dots in the middle of it would be
worse than ugly.

## The fonts and what they cost

Both fonts are drawn for this project — no third-party font, no graphics
library. Layout: one byte per pixel row, bit 0 on the left. Row 0 is free and
carries the mark above a letter (ä, é, ñ); the last row carries descenders
(g, j, p, q, y) and the cedilla under the c.

| Font | Glyph | Cell | Per line on 48 px | Bytes per glyph |
| --- | --- | --- | --- | --- |
| big | 5 × 9 | 6 × 10 | 8 | 9 |
| small | 3 × 7 | 4 × 8 | 12 | 7 |

149 glyphs each: 95 ASCII plus 54 for the five languages (à á â ä è é ê ë ì í
î ï ò ó ô ö ù ú û ü ÿ ñ ç ß œ, the uppercase forms, ¡ ¿ € « »). The 44 letters
with a mark are built by `generate.py` from the base letter plus the mark, so
only the base letters are drawn by hand.

**Flash cost, counted from the generated tables (not estimated):**

| Item | Bytes |
| --- | --- |
| big font, 149 × 9 | 1341 |
| small font, 149 × 7 | 1043 |
| codepoint table, 54 × 2 | 108 |
| **fonts together** | **2492** |
| texts of all five languages plus the keys | 2040 |
| pointer tables, 186 × 4 bytes on ARM | 744 |
| **texts together** | **2784** |
| **sum** | **5276 (5.2 KB)** |

Of the roughly 960 KB usable flash that is **0.55 %**. The mark above a letter
is a single pixel row, so acute and circumflex look similar at this size —
that is the price for keeping the fonts this small, and it is a deliberate
choice, not an oversight.

Code size is not measured for the target: there is no ARM toolchain in this
checkout, and the firmware build is skipped by `tools/check.py` without the
nRF Connect SDK. Compiled for x86-64 with `-Os`, the ten source files come to
**15.3 KB of code and 1.9 KB of data** (measured with `size` on 24.10.2026).
On Cortex-M4 that number is usually smaller, but **this is an indication, not
the target figure.**

## Five languages

`apps/firmware/lang/{de,en,fr,it,es}.json`, same keys in all five, 31 keys.
No visible text lives in the code. There is no file system on the device, so
`generate.py` turns the JSON into `osk_lang_table.h`, which goes into flash
with the firmware. The generated files are in the repository; `make` compares
them against a fresh run and stops if someone edited them by hand or changed a
language file without regenerating.

A new language is one more file in `lang/`, a regeneration, no code change.
If a text is missing, English is used — never an empty line, never the bare
key.

Three independent checks:

- `generate.py --check` fails if the five files do not have the same keys
  (the test Makefile calls it first).
- `tools/check.py` checks the same thing from the outside.
- `test_lang.c` checks what the firmware makes of it: five languages, every
  key resolves in every language, unknown language falls back to English, and
  every key the screens look up really exists.

## The button

One button, three gestures. Debounce 15 ms — the part (EVQP7A01P) is specified
at 10 ms maximum (`hardware/pinout.md`).

| Gesture | Time | Meaning |
| --- | --- | --- |
| short | < 800 ms | next page; on the decision screen: **no** |
| long | ≥ 800 ms | to the decision screen; there: **yes** |
| double | two shorts within 350 ms | read again from page 1 |

The long press is reported the moment the time is reached, while the finger is
still down, so nobody has to guess how long to hold. A short press is reported
only after the double-press window has passed — otherwise every double press
would also be a short one. The logic is pure arithmetic over "is it down" and
"what does the clock say", which is why it is testable without hardware,
including the 49-day clock wrap.

## How an approval runs

1. **Reading pages.** Each shows verbatim what this is about: service,
   recipient, amount, and the program that asked, if known. Short press pages
   on.
2. **Decision screen.** Long = yes, short = no, double = read again.
3. **Erasing asks twice** — two long presses on two decision screens.

A long press on a reading page does not approve; it only brings up the
decision screen. Nobody approves something they have not been shown.

Returns: `OSK_OK` after yes, `OSK_ERR_DENIED` when the user did not want it,
`OSK_ERR_TIMEOUT` when nobody was there (30 s by default, or
`req->timeout_ms`). **Without a working display there is no approval at all**:
`osk_ui_confirm()` returns `OSK_ERR_INTERNAL`, because the owner could not see
what they would be releasing.

`osk_ui_verify_user()` shows that the device is waiting for the PIN on the
computer and requires a long press as a presence check. There is no
fingerprint sensor in stage 1 (reasons in `docs/hardware.md`); with FIDO2
biometrics replace the PIN rather than adding a factor.

## State and the LED

| State | Key | LED |
| --- | --- | --- |
| locked | `status.locked` | red |
| unlocked | `status.unlocked` | green |
| working | `status.busy` | amber |
| battery low | `status.battery` | amber |
| error | `status.error` | red |

The LED repeats what the display says — as an addition, never as a
replacement. Anyone who cannot tell the colours apart reads the text.

## What is tested, and what is not

Run (gcc, address and undefined sanitizers, 24.10.2026):

```
apps/firmware/test/ui> make        -> 4094 Pruefungen, 0 Fehler   (48x64)
apps/firmware/test/ui> make wide   -> 4092 Pruefungen, 0 Fehler   (128x64)
```

The dummy records the bytes the driver sends, rebuilds the image from them —
subtracting the column offset again — and reads text back out of the pixels by
matching them against the font. So when a test says "the recipient is on the
screen", that is a statement about pixels, not about a buffer.

`tools/check.py` exits 0; it skips `firmware/tests` on Windows because neither
`make` nor `cmake` is installed there. The suite was run in WSL, as the two
older firmware suites are.

**Not tested, and not claimable:**

- No byte has ever gone to a real panel. The start sequence is read from the
  datasheet; whether this controller likes it is unverified.
- `osk_ui_port_nrf.c` (SPI, GPIO, LED, sleep) has **never been compiled**.
  It needs a devicetree overlay with the nodes `oled`, `oled_dc`, `oled_res`,
  `oled_pwr`, `osk_button` and `osk_led_*`, which does not exist yet.
- Contrast, readability, viewing angle, burn-in, current draw: nothing
  measured. The power budget in `docs/power-budget.md` is on paper.
- Button timings are checked against a clock that is a number. Whether 800 ms
  feels right in the hand is unknown.

## Open points

- Overlay and board glue, then the first real image: title, a long address
  over two pages, and the decision screen.
- Measure what a flush costs in time and current — the panel and the flash
  must not draw at the same time (`docs/hardware.md`).
- Decide whether the reading pages should advance on their own after a few
  seconds. Today only the button pages on, which is honest but slow for a
  long address.
