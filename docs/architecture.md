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
                   display shows it · approve with PIN + button
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

## Two key modes, chosen per profile

The first design said: one root secret in the chip, everything derived from it
**inside** the chip. That turned out to be impossible on this part. The SE05x can
derive bytes (HKDF, PBKDF2), but it returns them to the host — there is no command
that derives a key pair into an internal object, and `WriteECKey` only takes a
private value handed to it from outside. Deriving on the host would push the
private key through the nRF52840, whose read protection this document treats as
breakable. So that promise could not be kept, and it is gone.

Two modes exist instead, and a profile picks one:

**"this device only"** — the key pair is generated inside the chip, exported only
as an encrypted blob, carried in the credential ID, and imported again to sign.
The private part is never outside in the clear and nothing accumulates in the
chip's 104 kB. Cost: the wrapping key is tied to this device, so there is no
second device for these credentials.

**"mirrored"** — a root secret that moves to a paired second device over an
encrypted channel between the two secure elements, with derivation done in the
firmware. Cost: the secret is briefly in the nRF52840's memory. Benefit: a lost
stick can be replaced instead of ending every account bound to it.

Neither is strictly safer. Losing the only device destroys more accounts than a
stolen device usually does, so "mirrored" is the sensible default for everyday
logins — while a crypto seed belongs in "this device only".

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
