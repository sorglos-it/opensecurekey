# Hardware

Status: parts chosen on paper, nothing built or measured.

## Test setup (now)

| # | Part | Order code | Why |
| --- | --- | --- | --- |
| 1 | nRF52840-DK | PCA10056 | debugger on board, NFC antenna connector, all pins free |
| 2 | SE050 board | OM-SE050ARD-E | plugs straight onto the DK. `OM-SE050ARD` without `-E` is discontinued |
| 2b | cheaper option | MikroElektronika Secure 4 Click | SE050C2 on a small board |
| 3 | second device | GeeekPi nRF52840 MDK USB Dongle | target shape, UF2 bootloader; no debugger, only 12 GPIOs |
| 4 | NFC antenna | 13.56 MHz flex, 2-pin | the DK does not come with one |
| 5 | OLED | 0.96" SSD1306, SPI version | shows what is being approved |
| 6 | logic analyser | 8 channel USB | to prove the I2C line is encrypted |
| 7 | breadboard, jumpers, cables | — | — |

Roughly 190–220 €, or 150 € with 2b. Prices unchecked.

Not yet ordered: fingerprint sensor. It must do the matching itself, be available
in small numbers and have open documentation. Until one is found, the button is
the only confirmation.

## Target board (later)

- Raytac `MDBT50Q-1MV2` — nRF52840 in a pre-certified module, so the board itself
  does not need radio approval
- `SE050C2HQ1` (QFN20, 3 × 3 mm) — or the E variant, once its curves are confirmed
- QSPI flash 64 MB, e.g. `W25Q512JVEIQ` — vault plus the portable programs
- `MCP73831` charger, `DW01A` + `FS8205` protection, LiPo 100 mAh
- OLED, fingerprint sensor, button, RGB LED
- USB-C plug at the board edge, NFC antenna drawn as a trace

## Layout notes

- The battery must not sit behind the NFC antenna — it damps the field.
- Pad sizes come from the gerber, not from the library.
- Net flags get a stub, never glued to the pin.

## Open

- Energy budget: battery size against BLE idle, the current spike while scanning a
  finger, and the USB charge current.
- A LiPo in a keyring device lasts a few years. Replaceable, or stated as wear part.
- Whether the SE050E variant carries all curves — asked directly on the chip at
  first start, not assumed.
