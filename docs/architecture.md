# Architecture

Status: design. Nothing has run on hardware yet.

## Two chips, one job each

```
Computer / phone
  USB-HID   FIDO2 / CTAP2        ─┐
  USB-CCID  PIV, OpenPGP         ─┤
  USB-HID   keyboard (types)     ─┼─ applet layer (transport independent)
  USB-HID   vendor channel       ─┤          │
  USB-MSC   read-only drive      ─┤          │
  NFC       type 4 tag           ─┤          │
  BLE       GATT (our app only)  ─┘          │
                                             │
                display shows it · approve with finger or PIN + button
                                             │
                    ┌────────────────────────┴──────────────┐
              SE050 (I2C)                            Flash (QSPI, 64 MB)
              keys, certificates, signing            vault: AES-256-GCM
              never hands a key out                  + read-only drive image

              battery (LiPo, charges over USB) → NFC/BLE without a computer,
                                                  and the clock keeps running
```

The nRF52840 does the talking; the SE050 holds every secret. This split exists
because the nRF52840's own read protection has been defeated with voltage glitches
in the past. A broken nRF52840 leaks nothing on its own.

## Why a battery

The nRF52840 cannot harvest power from an NFC field — a YubiKey can, because there
a plain smart card chip answers. Here the firmware answers, so the device needs
its own power for NFC and Bluetooth. The battery also keeps the clock running,
which TOTP and timestamps need.

## Keys are derived, not stored

The SE050 has 50 kB of user memory. Storing one key per service would run out.
Instead a single root secret lives in the chip, and every FIDO credential, SSH key
and wallet address is derived from it inside the chip. That also makes a second
device possible: pair it, move the root secret, and it is a real spare.

## Folders

| Folder | Content |
| --- | --- |
| `apps/firmware/src/classes/se050` | I2C T=1', APDU, SCP03 |
| `apps/firmware/src/classes/ctap` | CTAP2 core and extensions |
| `apps/firmware/src/classes/applets` | PIV, OpenPGP, OATH, OTP |
| `apps/firmware/src/classes/store` | vault and device log |
| `apps/firmware/src/classes/transport` | USB, NFC, BLE |
| `apps/firmware/src/classes/wallet` | BIP32/39, PSBT, Ethereum |
| `apps/firmware/include` | the interfaces above — read these first |
| `shared/` | protocol and vault format |

## Interfaces

The headers in `apps/firmware/include/` are the contract between the modules.
They were written before the modules, so each part can be built and tested on its
own. Change a header only together with everyone who uses it.
