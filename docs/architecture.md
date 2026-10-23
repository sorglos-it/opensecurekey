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

**"this device only"** — one permanent key pair per credential, generated inside
the chip and never exported at all. The private part never exists outside: there
is no blob to steal and no unwrapping step. Cost: each credential occupies space
in the chip's 104 kB, so the number of services is finite — the interface warns
in good time (`osk_se050_free_memory()`) instead of failing at the limit. And
there is no second device for these credentials.

An earlier draft wanted to export the key as an encrypted blob and import it to
sign. That does not work and is recorded here so nobody tries it again: the
SE05x exports only *transient* objects, wraps them with chip-internal keys that
cannot be chosen, and refuses the import unless the original object still exists
(AN12413 3.2.8, 4.7.3.2, 4.7.1.8). It carries a value across a deselect — it does
not park credentials outside.

**"mirrored"** — a root secret that moves to a paired second device over an
encrypted channel between the two secure elements, with derivation done in the
firmware. Cost: the secret is briefly in the nRF52840's memory. Benefit: a lost
stick can be replaced instead of ending every account bound to it.

Neither is strictly safer. Losing the only device destroys more accounts than a
stolen device usually does, so "mirrored" is the sensible default for everyday
logins — while a crypto seed belongs in "this device only".

**The choice is made once and cannot be changed.** A secret born inside the chip
can never be mirrored afterwards, because nothing leaves the SE05x — not even
under a negotiated key (`ExportObject` cannot do it, `ReadObject` refuses private
and symmetric keys, 4.7.3.1). So a mirrored profile's root secret has to be
created *outside* both chips, during pairing, and written into each. That has to
happen before the secret exists.

For the setup wizard this means the question comes first, in plain words, and it
says that the answer is final. Changing your mind later means starting a new
profile and re-registering every service.

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
