# Radio: NFC and Bluetooth

The two wireless ways into the key: NFC (hold it against a phone) and
Bluetooth LE (talk to the management program without a cable).

**Both exist only while USB powers the stick.** The device has no battery (decided
25 October 2026), and the nRF52840 cannot harvest power from an NFC field. So the
stick lies in a USB port and the phone comes to it; holding a loose stick against
a phone, or talking Bluetooth to an own app with the stick on a key ring, does
not work. Nothing in the code below changes because of this — the transports do
not know where the power comes from. What changes is where they can be used. The
current draw is in `stromaufnahme.md`; the reasons for the decision are in
`architecture.md`.

## The honest limit

**No radio measurement has been made, and none can be made yet.** The board
is not built, so there is no antenna, no NFC field, no Bluetooth controller
and no phone that has ever seen this device. Everything described here is
**written and compiled**, and its logic has been exercised by tests on a PC
with made-up frames.

Of the four stages — written · compiled · in the image · executed on the
device — this code has reached the second, and the tests add "exercised on a
PC". Nothing more. In particular, none of the following is known:

- whether a reader's timing is met (the 800 ms recommendation, the frame
  waiting time, the extension requests),
- whether a phone opens the NDEF link on a tap,
- whether a browser or a phone's FIDO stack accepts this applet,
- whether pairing with a real Bluetooth stack produces the numbers this code
  expects, or how much power either way draws,
- whether a phone reads the stick well while it sits in a computer's USB port
  (antenna position, metal of the port and the housing — nothing is measured),
- whether a plain USB charger, without a computer, is enough to power an NFC or
  Bluetooth session (not examined).

What the tests do show is in the last section.

## Which specifications

| Specification | Where it is used |
| --- | --- |
| ISO/IEC 14443-4 (ISO-DEP), sections 7.1–7.5 | frames, chaining, block numbers, waiting-time extension, deselect |
| ISO/IEC 7816-4, section 5.1 and 11.1.1 | APDU cases 1–4 short and extended, command chaining, application selection |
| NFC Forum Type 4 Tag 1.1 | the tag's file system: capability container, NDEF file |
| NDEF 1.0, URI RTD 1.0, Text RTD 1.0 | the message that a phone reads on a tap |
| CTAP 2.1 Proposed Standard, section 11.3 | the FIDO applet: AID, selection, `NFCCTAP_MSG`, `NFCCTAP_GETRESPONSE`, `NFCCTAP_CONTROL`, status words (the section is unchanged in 2.2) |
| Bluetooth Core 5.4, Vol 3 Part H section 2.3 | LE Secure Connections, pairing method selection, numeric comparison |
| Bluetooth Core 5.4, Vol 3 Part F and G | ATT errors, GATT service and characteristics |

Nordic's SDK is a tool, not a source: `nfc_t4t_lib` and the BLE stack may be
used for layer 3 and the controller, but no example was copied — not its
code, not its structure, not its names.

## How it is layered

```text
  phone / reader                        management program
        │                                        │
   ISO 14443-3 (anticollision, RATS)        BLE controller
   — done by the nRF52840's NFC unit —      — SDK —
        │                                        │
   nfc_isodep.c   ISO-DEP: frames, chaining      ble_pair.c   pairing policy
        │                                        │
   nfc_tag.c      which application is selected  ble_gatt.c   GATT + fragments
        │     ╲                                       │
   nfc_ctap.c    nfc_ndef.c                      device protocol
   FIDO applet   tap-to-open / one-time code     (shared/protocol.md)
        │
   osk_ctap_handle  — the same core as USB
```

Each file holds one job, there is no dynamic memory anywhere in them, every
length is checked before it is read, and every buffer has a fixed upper
bound. An attacker with an antenna is closer to the device than one at the
USB port, so a frame that does not fit is rejected, never read in part.

## NFC: the FIDO applet

- The applet is selected by name with the AID `A0000006472F0001`. The answer
  is `U2F_V2`, because this device speaks both CTAP1 and CTAP2 — a CTAP2
  client then asks `authenticatorGetInfo`.
- `NFCCTAP_MSG` (CLA `80`, INS `10`) carries the command byte and the CBOR
  straight into `osk_ctap_handle` — **the same handler the USB side uses**.
  The transport adds nothing and removes nothing.
- Chained commands arrive with CLA `90` (and the ISO chaining bit `10` is
  accepted too) and are assembled before the core sees them.
- A response longer than one short APDU allows goes out in pieces: the first
  piece ends with `61xx`, and the client fetches the rest with `GET RESPONSE`
  (`00 C0`) or `NFCCTAP_GETRESPONSE` (`80 11`). An extended request gets an
  extended response in one go.
- `NFCCTAP_CONTROL` (`80 12 01 00`) deselects the applet; after that no CTAP
  command is answered until it is selected again.
- CTAP1/U2F commands (`00 01`, `00 02`, `00 03`) go to `osk_ctap_handle_u2f`
  as the raw APDU.

### Every operation needs the button — no exception

CTAP 2.1 says that holding the authenticator in the field counts as a user
gesture. **This device does not follow that.** `threat-model.md` names the
relay attack ("sits between phone and device") and answers it with the
button, and a device that releases a key because it is near a reader can be
used through a coat pocket. So:

- the core asks `osk_ui_confirm` for every operation, over NFC exactly as
  over USB, and the NFC transport does not bypass it;
- because a button press takes seconds, the transport asks the reader for
  more time first (`S(WTX)` with the largest allowed factor) — without that
  the reader gives up before the user has pressed;
- `authenticatorReset` stays refused over NFC, and it is refused **before**
  the button is asked, so that a reader cannot even provoke a prompt.

## NFC: tap without an app

Hold the key against a phone and the phone opens a stored address, or
receives a one-time code as text. No app needs to be installed — the phone
simply reads a tag, and the tag is us.

- **Off by default.** While the mode is `OSK_NDEF_OFF` the NDEF application
  does not exist: selecting it answers "file not found", with no hint that it
  could be switched on.
- Two modes: a stored address (URI record) or a one-time code (Text record).
  Where the code comes from is not this transport's business — it is handed
  in through a callback; the transport only types it.
- The file is **read-only**. Writing is answered with an error, not with
  silence, and the capability container says write access `FF`.
- **The button applies here too.** A one-time code is a secret and never
  leaves without a press. For the address a press is the default as well,
  because otherwise reading it out of a pocket would be possible; a user who
  wants the pure tap can switch the press off **for the address only**
  (`uri_without_button`). That switch has no effect on the one-time code.
- The file is built once per session, so a code cannot change between two
  partial reads — otherwise a phone would read two halves of two codes.

## Bluetooth: the management program, not FIDO

FIDO does **not** run over Bluetooth on this device. That is a decision, not
an omission: browsers do not support FIDO over Bluetooth reliably, and a path
that works sometimes is worse than one that is not offered. No CTAP handler
is registered for `OSK_LINK_BLE`.

What does run over it is the same protocol as the device's own USB channel
(`shared/protocol.md`), through one GATT service with three characteristics:
request (write), response (notify), status (read).

A message is sent in pieces, because the ATT maximum can be as small as 23
bytes: the first piece carries a marker byte and the total length, the
following ones a running number. Out-of-order pieces, a piece carrying more
than was announced, and a total length beyond the buffer are all refused with
an ATT error.

Nothing is answered unless the link is **encrypted, authenticated and bonded**
— each missing condition gets its own ATT error so the program can tell the
user what is wrong. If the security of a link drops while a message is half
assembled, the half message is discarded rather than finished on the weaker
link. Even the status characteristic needs the full conditions, because it
says how many devices are paired.

## Bluetooth: pairing

Enforced in code, not only documented:

1. **LE Secure Connections only.** Legacy pairing is refused, even if the
   other side can do nothing else.
2. **No "Just Works".** A peer that can neither display nor confirm is turned
   away — otherwise anyone in radio range could pair.
3. **Numeric comparison.** Six digits, with leading zeros, shown on the
   device's display and compared with the computer. Passkey entry would also
   be protected but is not the agreed method, so it is refused too.
4. **Encryption is mandatory**, and the stack must report both encryption and
   authentication before anything is stored.
5. **The button comes first.** Without a press there is not even a number to
   look at, and the list is checked for room before the user is asked.
6. **Session-only pairing is refused**: an access the user cannot see in the
   list and cannot delete is not offered.
7. **A rotating address** (resolvable private address, 15 minutes) so the key
   cannot be followed around.
8. **Paired devices are visible and individually deletable.** Four slots, a
   fixed array, no dynamic memory; a deleted slot is overwritten, not just
   released. A peer's name comes from outside, so it is bounded and stripped
   of anything that is not printable before it reaches a display.

## What the tests actually show

`apps/firmware/test/transport/`, built and run on a PC with `make`:

| Suite | Checks | What it exercises |
| --- | --- | --- |
| `test_apdu` | 55 | all four APDU cases, short and extended, and the malformed forms |
| `test_isodep` | 102 | real ISO-DEP frames: chaining both ways, block numbers, retransmission, CID, deselect, waiting-time extension, timeout, overflow |
| `test_ndef` | 112 | the NDEF parser against lying lengths, the file builder, the tag application, and that the button is required |
| `test_ble` | 245 | the pairing policy (every refusal case), the bond list, address rotation, GATT access control and fragment assembly |
| `test_nfc_chain` | 811 | application selection and the FIDO applet with a stand-in core, so that response chaining (`61xx`) can be forced |
| `test_nfc` | 42 | the **real** CTAP core over NFC |

`test_nfc` is the one that proves the three claims that matter:

- the same CBOR answer, byte for byte, when `authenticatorGetInfo` is asked
  over NFC and over USB HID — so it really is one core,
- a U2F registration over NFC fails without a button press and succeeds with
  one, and asks exactly once,
- `authenticatorReset` is refused over NFC and accepted over USB, and over
  NFC no button prompt happens at all.

`make fuzz` runs two fuzzers, both with ASan and UBSan compiled in:

- `fuzz_isodep` — random and half-plausible frames into the whole NFC stack.
  It asserts that no frame leaves longer than the frame size, and it fails if
  the core was never reached or no answer ever carried `9000` — a run that
  only bounces off the first wall is not a run.
- `fuzz_ndef` — NDEF messages with lying length fields. For every record the
  parser reports, the fuzzer re-checks that type, id and payload lie wholly
  inside the message, and that the parts do not add up to more than the
  message.

Measured on 2026-10-24, 20 seconds each, warm machine, with
`-fsanitize=address,undefined`: `fuzz_isodep` 108,043,091 frames in,
237,520 core calls, 1,443,130 answers with `9000`; `fuzz_ndef` 56,810,873
messages, 13,434,988 of them parsed, 25,274,602 records. Those numbers say
the fixtures were running — they say nothing about radio.

## What is deliberately not here

- Layer 3 (anticollision, RATS/ATS): the nRF52840's NFC unit does it. This
  code starts at the first block after the RATS, and the reader's maximum
  frame size has to be handed in (`osk_isodep_set_frame_size`).
- The CRC of a frame: the NFC unit computes and checks it.
- Generating the one-time code, and the device protocol itself: both are
  handed in through callbacks, so this transport holds no secrets of its own.
- FIDO over Bluetooth, for the reason above.
