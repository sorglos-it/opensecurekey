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

              no battery: everything runs from USB, NFC and BLE included
              (they exist only while the stick is plugged in)
```

The nRF52840 does the talking; the SE050 holds every secret. This split exists
because the nRF52840's own read protection has been defeated with voltage glitches
in the past. A broken nRF52840 leaks nothing on its own.

## Why no battery

On 25 October 2026 the battery was dropped altogether: no cell, no charger, no
protection circuit. The device runs from USB only.

The nRF52840 cannot harvest power from an NFC field — a YubiKey can, because
there a plain smart card chip answers. Here the firmware answers, so NFC and
Bluetooth need the stick to be powered. **They now exist only at the computer.**
That was accepted on purpose.

The reasons the battery went:

- **The clock needs no continuous supply.** For time-based one-time codes (TOTP)
  the clock is set from the computer when the stick is plugged in, and the code
  is made at the moment of use. Timestamps on signatures take the time from a
  timestamp server, not from the device.
- **What was left to buy** was NFC and BLE without a computer. Against that stood
  more length (how much is in `hardware/layout-notes.md`) and a consumable with a few years of life in a device meant
  to last decades.

**A consequence for the firmware:** the device loses its supply at the moment it
is pulled out, and there is no buffer any more. A write to the flash or to the
security chip that has begun must survive that. Nothing in the code has been
checked against it yet.

Open: who hands the time to the device when it is plugged in is not decided (see
`dongle-firmware.md`), and a wrong computer clock sets a wrong device clock. The
current draw that remains is in `docs/stromaufnahme.md`; the cell study that
ended with this decision is in `docs/zellen.md`.

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
in the chip's memory (46 kB or 104 kB, depending on the ordering option), so the number of services is finite — the interface warns
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

**A real SE051 may lack what "mirrored" was going to use.** The feature bits are
set at the factory and cannot be changed afterwards, and the one FIPS-validated
part we have evidence for ships with EdDSA and Curve25519 **off** and with FIPS
mode available — and in FIPS mode the part refuses ECDH outright (AN12413
4.10.3.1). The variant table says SE051C2 *can* do these; the delivered part is
what counts, and one call to `osk_se050_open()` answers it.

That does not sink the mirrored mode, because its root secret is created on the
host anyway — see below. What it does sink on such a part is using the chip's
own ECDH to secure the pairing channel: that key agreement would have to run in
the firmware instead. For a transport key that exists for one pairing, that is
acceptable; for the server-to-server use in `threat-model.md` it is not, because
there the private key is supposed to stay in the chip for years. On a FIPS part
that use case is simply unavailable, and the program must say so rather than
fail with something that looks like a missing PIN.

**The choice is made once and cannot be changed.** A secret born inside the chip
can never be mirrored afterwards, because nothing leaves the SE05x — not even
under a negotiated key (`ExportObject` cannot do it, `ReadObject` refuses private
and symmetric keys, 4.7.3.1). So a mirrored profile's root secret has to be
created *outside* both chips, during pairing, and written into each. That has to
happen before the secret exists.

For the setup wizard this means the question comes first, in plain words, and it
says that the answer is final. Changing your mind later means starting a new
profile and re-registering every service.

## How much room is there, really

Four separate memories, and only one of them is tight.

| Memory | Size | Holds | Pressure |
| --- | --- | --- | --- |
| QSPI flash | 64 MB | vault, device log, the portable programs | none |
| Secure element | 46 or 104 kB | keys and certificates | limits credentials in "this device only" |
| nRF52840 RAM | 256 kB | stacks, buffers, BLE | to be measured |
| **nRF52840 flash** | **1 MB** | **the firmware itself** | **this is the tight one** |

The firmware shares that 1 MB with the bootloader and — if the update slot sits
inside — with a second copy of itself. That would leave roughly 496 kB per image,
and Zephyr with the BLE controller, USB, NFC and the crypto library is already a
large part of that. Today's sources are about 12,000 lines of C and nothing of
PIV, OpenPGP, the wallet, the display or the radio transports is written yet.

**So the update slot goes into the external flash, not into the internal one.**
MCUboot can keep its secondary slot in the QSPI flash, which there is 64 MB of.
That turns "about 496 kB" into "about 960 kB" and costs nothing but a partition
layout decided early — which is why it is written down now and not when the
first build fails to link.

What is *not* a way out: running code directly from the external flash (XIP).
The nRF52840 nominally supports it, but erratum 216 limits it, Nordic advises
against relocating interrupts there, and the published examples target the
nRF5340. Treat XIP as unavailable on this part.

If even 960 kB turns out to be too little, the honest answer is a different
part (the nRF5340 has two cores and real XIP support), not a trick. That
decision would come before the board is made, not after.

### Measured, 25 October 2026: it fits, with room to spare

The first build for the real target is done. Board `nrf52840dk/nrf52840`,
79 source files, fresh build directory, cross-checked with a second tool:

| | used | region | against the 960 kB |
| --- | --- | --- | --- |
| **nRF52840 flash** | **161 124 B** (157 kB) | 1 MB, 15.37 % | **16.4 %** — 802.7 kB free |
| **nRF52840 RAM** | **97 100 B** (95 kB) | 256 kB, 37.04 % | — |

**And a finding that changes the reasoning above:** at 157 kB the firmware
would also have fitted into the roughly 496 kB that an update slot *inside*
the chip would have left. **Moving the slot to the external flash was not
necessary.** It does no harm and it stays — but the headroom is far larger
than planned, and the internal flash is not as tight as this section assumed.

What the number is worth, and what it is not:

- **The image does not contain all of our own code.** Read from the ELF
  symbols: 243 of our 535 functions are in it; the linker discarded 292
  with `--gc-sections`, because `src/main.c` does not reach them. Our code
  contributes 30 kB to the image but amounts to 137 kB in total. With all
  of it reachable the image would be roughly **264 kB — 27.5 % of the
  960 kB**, still leaving close to 700 kB. The answer stays "it fits"; the
  margin is simply smaller than 16.4 % suggests.
- It is a **lower bound** for a second reason: six symbols do not exist on
  the device yet, and the measurement build substitutes placeholders for
  them (`apps/firmware/src/messbau.c`). The real implementations are bigger.
- USB, BLE and NFC on the Zephyr side are not written yet, nor is the
  bootloader. Those are the large items still to come.
- It is **stage 2 of 4**: compiled and in the image, **never run on the
  device**. There is no hardware yet.
- The other numbers in the table above — QSPI flash and secure element —
  are still the data-sheet sizes, unmeasured.

How it was measured, every finding, and the traps on the way there:
`docs/build.md`, section "Der erste Bau für ARM".

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
