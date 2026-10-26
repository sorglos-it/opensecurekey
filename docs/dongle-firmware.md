# The device side of the dongle and server commands

This page covers what the firmware answers for commands 14 to 18 of
[`shared/protocol.md`](../shared/protocol.md) — the device half of
[`docs/dongle.md`](dongle.md), which describes the host half.

Status: written, compiled and run on a PC against mocks. **Nothing has run on
a device.** The board is not built, no chip was ever involved, and the
signatures in these tests are the right shape, not the right curve. What that
means in detail is in "What was measured, and what was not" below.

Code: `apps/firmware/src/classes/applets/guard/`.
Tests: `apps/firmware/test/guard/`.

## The one rule that holds the rest together

The device puts the purpose string in front. Not the computer.

```
attest with bh:  "osk-guard/attest/1"     0x00 nonce(32) programHash(32)
attest without:  "osk-guard/attest-raw/1" 0x00 challenge(1..105)
sign:            "osk-guard/sign/1"       0x00 label 0x00 jobHash(32)
```

The three strings are constants in `osk_guard_msg.c`. There is no field in
any request that selects one, and no request can supply one. A firmware that
signs whatever it is handed breaks the separation no matter how careful the
host is — so the separation lives here, in the device, and not in the
agreement between host and device.

Why the old `attest` call needs its own purpose: both attest paths would
otherwise sign `purpose 0x00 <bytes>`. A 64-byte challenge is exactly as long
as nonce plus program hash, so calling the old path with
`nonce||programHash` would return a signature the dongle check accepts, and
the binding to the program file would be gone. That hole was found by the
host strand while making the shared simulator sign for real. The same
arithmetic separates `attest` from `sign`.

`test_forgery.c` stages that attack against the firmware and compares bytes.
`mutanten.py` merges the purposes on a copy of the tree and requires the case
to go red — see the measurement below.

## The five commands

| `c` | What the firmware does | Asks on the device |
| --- | --- | --- |
| 14 `attest` | signs the challenge with the device key, returns `sig` and `pub` | configurable, off by default |
| 15 `derive` | `osk_se050_hkdf()` from the root secret | no |
| 16 `agree` | `osk_se050_ecdh()` with a named X25519 key | grant, or a button press |
| 17 `session` | switches the grant on and off, reports its state | yes, to switch on |
| 18 `sign` | signs a job hash with a named Ed25519 key | grant, or a button press |

`attest` answers with the vault closed: a protected program asks "are you
there?", and that needs no secret. Everything else uses a key and requires
the vault open, otherwise `OSK_ERR_LOCKED`. Commands outside 14 to 18 get
`OSK_ERR_UNSUPPORTED` — 1 to 13 belong to a handler that does not exist yet.

### 14 `attest` and the button

`attest` does not ask by default (`osk_guard_set_attest_confirm()`). A
protected program asks about once every two seconds; demanding a press each
time teaches the user to dismiss presses without looking, and that destroys
the value of every later real question. `attest` releases no secret: it signs
a random value under a purpose that fits nothing else.

### 15 `derive` never returns the root secret

The command only ever calls `osk_se050_hkdf()`. There is no path in
`osk_guard_keys.c` that returns the root secret, and the chip has no command
for it either (`ReadObject` refuses symmetric and private keys, AN12413
3.7.1). A test derives every allowed length (1 to 64 bytes) and compares each
answer with the mock's root secret.

### 16 `agree` on a part that cannot do ECDH

A delivered SE051 configured for FIPS mode at the factory refuses ECDH
outright (AN12413 4.10.3.1). The command passes `OSK_ERR_UNSUPPORTED`
straight through. It is deliberately **not** `OSK_ERR_DENIED`, which would
look like a missing PIN, and the firmware deliberately does **not** compute
the agreement itself: the key would then live in the nRF52840's memory, where
a copy of the server could reach it, and that is the withdrawal of the only
promise the server case makes ([`docs/threat-model.md`](threat-model.md)).
Signing is unaffected on such a part, and a test covers both halves.

### 17 `session`: the grant, and its fences

| Fence | How the firmware enforces it |
| --- | --- |
| Off by default | the grant table lives in RAM and starts empty; `osk_guard_init()` clears it, and a power cut or an unplug clears it too |
| Explicit | switching on always calls `osk_ui_confirm()` — a running grant never covers switching on again |
| Named keys only | a grant lists its labels, and only labels that have a key (otherwise `OSK_ERR_NOT_FOUND`, so nobody gets a promise into the void) |
| Time-limited | at most 24 hours; a longer `ttl` is **refused**, not silently shortened, because a silent cut makes the operator believe they have more |
| Expires by itself | entries expire when looked at — there is no timer that could fail |
| Logged | the grant and every use go into the device log |
| Not reachable from the library | the firmware accepts command 17 only on the management channel (`OSK_LINK_USB_VENDOR`); on any other transport it answers `OSK_ERR_DENIED`, even with a correct frame |

That last row is the one worth stressing: on the host side it is the absence
of a call in `osk-guard`, which an attacker who writes their own frames can
ignore. On the device side it is a rule, and a linked-in library cannot lower
the bar for the whole machine by going around its own API.

Switching off needs no confirmation. Raising the bar is never the dangerous
direction.

### 18 `sign` and the named keys

A label like `prod-eu-1` points at its own object in the chip. The object
number is **not** computed from the label: two labels with the same short
hash would silently share a key, and a signature for `test` would be one for
`prod`. An unknown label gets `OSK_ERR_NOT_FOUND` and never a key — and it
gets no button press either, so the user never dismisses a question about a
name the device does not even know.

Labels are registered by setup (`osk_guard_node_register()`), not by any of
commands 14 to 18: a server must not be able to issue itself new identities.

**Open point for the master thread:** the label-to-object table currently
lives in RAM, so after a reboot the labels have to be registered again while
the chip objects are still there. Where that table is persisted — a vault
item read once when the vault opens, or a small dedicated area — is a
decision that touches the store strand, and it is not made here.

**Second open point:** the log entry carries the device clock, which counts
from power-on, not a real UTC time. Only the computer can supply UTC, and who
hands it over is not decided yet. Since the battery was dropped (25 October
2026) the clock always starts at zero when the stick is plugged in, so the
computer has to hand it over at every plug-in; there is no clock that keeps
running between two uses. Until then the log says device time, and it
says so here rather than letting a reader assume otherwise.

## Why there is a table and not a derivation

The contract asks for a per-label key pair derived inside the chip. **The
SE05x cannot do that.** Its two derivation commands return the derived
material to the host, which is why `osk_se050_key_derive_transient()` answers
`OSK_ERR_UNSUPPORTED` ([`docs/se050.md`](se050.md)). The obvious route —
derive out, write back in — walks the private key through the nRF52840, whose
read protection this project treats as broken. So: one persistent object per
label, created at setup.

## Limits, all of them fixed

| Limit | Value | Why |
| --- | --- | --- |
| challenge of the old `attest` | 105 bytes | Ed25519 goes into the chip as the whole **message** (AN12413 4.10.1.2) and the driver takes at most 128; purpose plus separator eat 23. Longer is refused — hashing it would make the device sign different bytes than the verifier expects. The host allows up to 256, so this is a device limit the host must respect. |
| label | 64 bytes, no 0x00 | same rule as the host (`NodeKeyLabelMax`); a zero byte would make the signed bytes ambiguous |
| `derive` output | 1 to 64 bytes | same as the host |
| `derive` info | 128 bytes | fixed buffer |
| named keys | 8 | each one permanently occupies chip memory (104 kB on the SE051C2) |
| labels per grant | 4 | "name one key" is the recommended way to run this |
| grant duration | 86400 s | the 24 hours promised in `dongle.md` |

Every length is checked before anything is read, every buffer is a fixed
field, and there is no dynamic memory anywhere in the crypto path.

## The frame

A CBOR map with `v`, `c`, `s`, `d`; the answer carries `v`, `s`, `r` and `d`.
`r` is the numeric value of `osk_result_t`, which is the numbering the host
already uses. `d` may be missing, and then every mandatory field is missing,
which gives the same answer as an empty `d`.

The reader is the strict one from `osk_cbor.h` — every length checked before
reading, no dynamic memory, fixed bounds. One thing it does **not** demand is
the canonical key order: the host sorts its keys by text, CTAP sorts by
length first (`"n"` before `"bh"`), so insisting on CTAP order would be a
wall against our own host. Duplicate keys are refused, unknown ones skipped.

## What was measured, and what was not

On 24.10.2026, in a worktree at `D:\claude\opensecurekey-guardfw`, with gcc
under WSL and address/undefined sanitizers on:

```
make -C apps/firmware/test/guard
```

- `test_msg` — 35 cases, the signed byte strings alone, no chip, no frame
- `test_guard` — 345 cases, frame, `attest`, `derive`
- `test_node` — 114 cases, `sign`, `agree`, `session`
- `test_forgery` — 16 cases, the forgery staged against the firmware
- **510 cases, 0 errors, exit code 0.** Nothing was skipped.

Mutation controls (`python3 apps/firmware/test/guard/mutanten.py`): four
mutants that must die, one that must live. Each runs in its own copy of the
tree, never in the working tree, and the script verifies that its own
replacement actually took effect rather than reporting a number.

The mocks (`test/guard/mocks.c`) are honest about what they are: a
"signature" is HMAC-SHA256 over the message, chained to 64 bytes. That has
the right shape and length, not the right value. For the purpose separation
that is exactly enough, and not by luck: the mock is a function of the
**signed bytes**, so equal signatures mean equal messages. Real Ed25519 is
deterministic too (RFC 8032), so the property the test leans on holds there
as well. What the mock does not prove is any curve arithmetic — that is the
chip's job, later.

Not measured, and not claimed:

- **No device, no chip.** The board is not built. Everything above is true of
  the code and of the mocks.
- **Not built for ARM.** The PC test suite compiles these files with gcc. The
  firmware build for the nRF52840 is being set up by another strand; whether
  these files compile there is open.
- **No handler for commands 1 to 13**, so the applet has never been exercised
  as part of a complete device dialogue — only command by command.
- **Not run against the host side.** The Go client and `osk-guard` were
  measured against the simulator, this applet against its own mocks. Nobody
  has yet put the two ends on one wire; the frame and the field names are the
  same on paper, and that is a claim about two documents, not a measurement.
- **No audit.** Nobody outside this project has looked at the crypto.
