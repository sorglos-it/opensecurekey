# Protocol between device and management program

This describes the private channel (USB vendor HID, and the same messages over
BLE). It does **not** describe FIDO2, PIV or OpenPGP — those are public standards
and the device simply follows them.

Status: design. Nothing has been implemented or measured yet.

## Frame

Every message is a CBOR map inside a 64 byte HID report chain:

| Field | Type | Meaning |
| --- | --- | --- |
| `v` | uint | protocol version, starts at 1 |
| `c` | uint | command, see below |
| `s` | uint | sequence number, echoed in the answer |
| `d` | map | command data |

The answer carries the same `s`, plus `r` (result, 0 = ok) and `d`.
A device with an older protocol answers `OSK_ERR_UNSUPPORTED` for unknown `c`;
the program then says in plain words which function is missing.

## Commands

| `c` | Name | Needs approval on device |
| --- | --- | --- |
| 1 | `hello` — version, serial, which transports are on | no |
| 2 | `unlock` — PIN or fingerprint session | yes |
| 3 | `lock` | no |
| 4 | `list` — item metadata, never secrets | after unlock |
| 5 | `reveal` — one secret | yes, every time |
| 6 | `add` / 7 `update` / 8 `delete` | yes |
| 9 | `log` — read device log | after unlock |
| 10 | `pair` — pair a second device or a host | yes, number comparison |
| 11 | `backup` / 12 `restore` — encrypted blob | yes |
| 13 | `update` — signed firmware image | yes |
| 14 | `attest` — challenge/response for dongle use | configurable |

## Setup: one question that cannot be taken back

Before a device holds any secret, the wizard asks which mode the profile uses —
"mirrored" (a spare device is possible) or "this device only" (nothing ever
leaves the chip). The answer decides how the root secret is created, so it has
to be given first and cannot be changed afterwards; see `docs/architecture.md`.

The program must put this in plain words, say that it is final, and not offer a
default that quietly decides it. `hello` reports the mode of each profile so the
interface can show it without asking the device to reveal anything.

## Rules

- The program never sees a key. It sees metadata, and secrets only through
  `reveal`, one at a time, after the user pressed the button.
- Every `reveal` is written to the device log with the name of the asking program.
  That name is a claim by the program, not proof — it is shown as such.
- The device decides what is allowed, never the program. A program that asks for
  something switched off gets `OSK_ERR_DENIED`, not a silent success.
- Secrets are wiped from the program's memory right after use.
