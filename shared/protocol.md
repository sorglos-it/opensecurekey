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
| 4 | `list` — item metadata, never secrets; carries `pub` for SSH keys, certificates and PGP keys | after unlock |
| 5 | `reveal` — one secret | yes, every time |
| 6 | `add` / 7 `update` / 8 `delete` | yes |
| 9 | `log` — read device log | after unlock |
| 10 | `pair` — pair a second device or a host | yes, number comparison |
| 11 | `backup` / 12 `restore` — encrypted blob | yes |
| 13 | `update` — signed firmware image | yes |
| 14 | `attest` — challenge/response for dongle use: `ch` nonce(32), `bh` program hash(32) → `sig`, `pub` | configurable |
| 15 | `derive` — `info`, `n` → `key`; never returns the root secret | no |
| 16 | `agree` — `k` label, `pub` peer(32) → `sec`; may answer `OSK_ERR_UNSUPPORTED` on a part without ECDH | yes, or grant |
| 17 | `session` — `on`, `k` labels, `ttl`, or `ask` → state (the server grant) | yes, to switch on |
| 18 | `sign` — `k` label, `h` hash(32) → `sig`; or `pub`+`cv` for the public part | yes, or grant |

**Not built yet: the firmware side of 14–18.** The management program and
`osk-guard` speak these commands and are tested against the simulator; the
device does not answer them. Whoever builds that side must keep the purpose
strings apart — a forgery was found while the simulator was being made to sign
for real: the old attest call would have produced a valid dongle answer from a
64-byte challenge.

`attest` and `sign` are deliberately separate, and the device puts a distinct
purpose string in front of each before signing. A firmware that signs whatever
it is handed breaks that separation no matter how careful the host is.

## Setup: one question that cannot be taken back

Before a device holds any secret, the wizard asks which mode the profile uses —
"mirrored" (a spare device is possible) or "this device only" (nothing ever
leaves the chip). The answer decides how the root secret is created, so it has
to be given first and cannot be changed afterwards; see `docs/architecture.md`.

The program must put this in plain words, say that it is final, and not offer a
default that quietly decides it. `hello` reports the mode of each profile so the
interface can show it without asking the device to reveal anything.

## How much room is left

The chip reports its free persistent memory, and the interface warns before the
limit instead of failing at it. Two limits of that number, which the interface
must respect:

- Anything from 32768 bytes upwards is reported as `0x7FFF` and nothing more. On
  a fresh SE051C2 with 104 kB the answer is therefore only "plenty". A real
  figure appears once the chip is more than half full — which is when a warning
  matters, so it is enough to warn with, but it is a lower bound, not a measurement.
- The granularity is 16 bytes and the applet reports the worst case. So do not
  divide by an object size and show "41 services left". That would be invented
  precision. "Running out of room" is what the value supports.

## Rules

- The program never sees a key. It sees metadata, and secrets only through
  `reveal`, one at a time, after the user pressed the button.
- Every `reveal` is written to the device log with the name of the asking program.
  That name is a claim by the program, not proof — it is shown as such.
- The device decides what is allowed, never the program. A program that asks for
  something switched off gets `OSK_ERR_DENIED`, not a silent success.
- Secrets are wiped from the program's memory right after use.
