# Smart card side: PIV, OpenPGP, one-time passwords

What the key does when a computer treats it as a chip card.

Status: written, compiled and run on a PC with gcc 14 and clang 19, 564 test
cases green, fuzz runs clean under AddressSanitizer and UndefinedBehaviorSanitizer.
**Nothing has run on the device, and nothing has run against a real smart card
stack** — not pcscd, not Windows, not macOS, not GnuPG, not OpenSC, not
KeePassXC, not VeraCrypt. The board is not built yet, so every test runs
against stand-ins for the SE050, the vault and the display.

## Why CCID

The key reports itself as a USB chip card reader (class 0x0B) with a card
permanently inserted. Windows, Linux (pcscd) and macOS all ship that driver,
so PIV and OpenPGP work with nothing installed. That is the whole reason this
route was chosen over a vendor channel.

Exchange level is **short APDU** (`dwFeatures` bit 0x00040000). Anything longer
than one short response travels by GET RESPONSE and command chaining, which
both specifications require anyway.

## Which specification

| Specification | Version used |
| --- | --- |
| USB CCID | Revision 1.1, 22 April 2005 |
| Command and response format, BER-TLV | ISO/IEC 7816-4 |
| Answer to reset | ISO/IEC 7816-3, section 8 |
| PIV | NIST SP 800-73-4, parts 1 and 2 |
| OpenPGP card | Functional specification 3.4, 8 July 2020 |
| SHA-1 · HMAC · AES-128 | FIPS 180-4 · RFC 2104 · FIPS 197 |
| CRC-16 | ISO/IEC 13239 (reversed polynomial 0x8408) |

Test vectors come from those documents: RFC 3174 for SHA-1, RFC 2202 cases 1, 2, 3 and 6
for HMAC-SHA1, FIPS 197 appendices B and C.1 for AES.

## What works

### USB-CCID (`src/classes/transport/usb_ccid.c`)

| Message | State |
| --- | --- |
| `IccPowerOn` / `IccPowerOff` / `GetSlotStatus` | done, card is always present |
| `XfrBlock` | done, hands the APDU to the applet router |
| `GetParameters` / `ResetParameters` / `SetParameters` | done, T=1, answers with our own values |
| `Abort`, `IccClock`, `SetDataRateAndClockFrequency` | answered, nothing is changed |
| `Escape`, `Secure`, `T0APDU`, `Mechanical` | rejected with "command not supported" |

The class descriptor (54 bytes) and the ATR are built in code, including the
ATR checksum, and a test recomputes both. A message whose announced length does
not match the delivered length is refused; a message without a complete 10-byte
header gets no answer at all, because there is nobody to answer to.

### Applet router (`src/classes/applets/osk_applets.c`)

One handler is registered per transport, but a card carries several
applications. The router selects by application identifier (full or
abbreviated), and takes three jobs off both applets: command chaining
(CLA bit 5), response chaining (61xx plus GET RESPONSE) and rejecting
secure messaging and logical channels other than 0.

### PIV (`src/classes/applets/osk_piv.c`)

| Command | State |
| --- | --- |
| SELECT | done, returns the application property template |
| VERIFY (0x80) | done, including the empty query and P1 = 0xFF |
| CHANGE REFERENCE DATA (PIN, PUK) | done |
| RESET RETRY COUNTER | done, PUK from the vault |
| GET DATA | done, certificates 9A/9C/9D/9E, CHUID, CCC, discovery object |
| PUT DATA | done, needs the PIN and a button press |
| GENERATE ASYMMETRIC KEY PAIR | done, P-256 and P-384 |
| GENERAL AUTHENTICATE | done, signing (tag 81) and key agreement (tag 85, slot 9D only) |

Keys live in the SE050 and never leave it; certificates live in the vault as
`OSK_ITEM_CERT`. Slot 9C enforces "PIN always": the PIN must be verified in the
command directly before the signature, and a second signature without a fresh
VERIFY is refused. Slot 9E needs no PIN, as the specification says — but it
still needs the button.

### OpenPGP (`src/classes/applets/osk_openpgp.c`)

| Command | State |
| --- | --- |
| SELECT | done (returns no data, per section 7.2.1) |
| VERIFY (PW1 81/82, PW3 83) | done, including query and P1 = 0xFF |
| CHANGE REFERENCE DATA (PW1, PW3) | done |
| RESET RETRY COUNTER | done, with PW3 (P1 = 02) and with the resetting code (P1 = 00) |
| GET DATA | 4F, 5E, 5F50, 5F52, 65, 6E, 7A, C0, C4 |
| PUT DATA | 5B, 5F2D, 5F35, 5E, 5F50, C1 to C4, C7 to C9, CA to CC, CE, D3 |
| GENERATE ASYMMETRIC KEY PAIR | done, generate (P1 = 80) and read (P1 = 81) |
| PSO: COMPUTE DIGITAL SIGNATURE | done, output is flat r ‖ s per RFC 6637 |
| PSO: DECIPHER | done, ECDH from A6 → 7F49 → 86 |
| INTERNAL AUTHENTICATE | done |
| TERMINATE DF / ACTIVATE FILE | done, wipes keys and card data |

PW1 is valid for one signature unless the PW status byte says otherwise, and
the signature counter in DO 93 counts along. Card holder data, fingerprints,
the counter and the PW status survive a power cut: they sit in one vault record
that is read once per power-up and written after every change.

### One-time passwords (`src/classes/applets/osk_otp.c`)

Two slots. Slot content is a vault item (`OSK_ITEM_API_KEY`).

- **Yubico OTP**: 44 characters of ModHex — 12 for the public identity, 32 for
  an AES-128 block holding private identity, use counter, timestamp, session
  counter, randomness and checksum. The counters sit in the vault record, so
  they survive a power cut; the new counter value is written **before** the
  password is built, so a power cut loses a password rather than repeating one.
- **Challenge-response (HMAC-SHA1)**: 1 to 64 bytes in, 20 bytes out.

The test does not merely check the shape: it builds the whole block itself,
encrypts it and compares character for character, with the randomness made
predictable on both sides.

### Every release is visible

Each signature, each key agreement, each key generation and each write calls
`osk_ui_confirm` with the name of the service. Releasing a secret from the
vault goes through `osk_store_reveal`, which asks by itself — so reading a
certificate or an OTP secret also puts a name on the display. Tests count the
questions and read back the subject line; when the user says no, nothing
comes out.

Certificates are cached in RAM for one power cycle after the first release.
Without that, a program like Firefox enumerating certificates would ask for a
button press per certificate at every start.

## Decisions worth knowing

**One PIN, held in the chip.** PIV's PIN and OpenPGP's PW1 are the same device
PIN, verified by the SE050, with the retry counter inside the SE050. A second
PIN kept in firmware would have a counter that resetting the power resets, so
it would be weaker, not safer.

**The administrative secrets live in the vault.** The chip has exactly one PIN,
so the PIV PUK, the OpenPGP admin PIN (PW3) and the resetting code sit in the
vault, each with its retry counter in the same record — in flash, not in RAM.
The counter is decremented and written **before** the comparison, so cutting
the power at the right moment does not buy a free attempt. Verifying one of
them opens a vault record and therefore asks on the display, which is correct:
every one of these is an administrative act.

**No PIV management key (9B).** PIV protects writing with a 3DES management
key. This device has no 3DES, so PUT DATA and key generation require the
verified PIN plus a button press instead.

**Elliptic curves only.** P-256 and P-384 for PIV, P-256 and P-384 for OpenPGP.
`osk_se050.h` offers no RSA, so RSA keys are answered with "function not
supported" rather than pretended.

**SHA-1 and single-block AES are in here on purpose.** Both are old, and AES
without a mode of operation is not a general building block. They are here
because the other side — the Yubico validation service, KeePassXC, VeraCrypt —
understands nothing else. Everything new uses the SE050. The HMAC-SHA1 secret
is kept in the vault rather than in the chip, because `osk_se050_hmac()` names
no hash algorithm and SHA-1 cannot be assumed to be there.

**No dynamic memory anywhere.** Fixed buffers, fixed upper bounds, no malloc,
no alloca, no recursion. The command cap is 3072 bytes, which a certificate
fits inside; anything larger is refused rather than partly read.

## What is missing

- **The Yubico HID report protocol.** KeePassXC and VeraCrypt talk to a YubiKey
  through its own USB-HID feature reports. That framing is not emulated, so
  **those two programs do not work with this build yet** — only the engine
  behind them does (ModHex, the OTP block, HMAC-SHA1), verified against the
  specifications' own vectors. The framing is not written from any published
  specification, and rebuilding it from somebody else's source is not an option
  here, so it waits for a description that can be followed cleanly.
- **The keyboard transport.** `osk_otp_generate()` hands out the 44 characters;
  typing them belongs to the USB keyboard transport, which is another strand's
  folder.
- **The serial number in the OpenPGP AID is the same on every device.**
  GnuPG tells cards apart by it, so two of our sticks on one computer look like
  one card. A real serial needs a production step that does not exist yet.
- **PIV management key (9B), biometrics, card-level secure messaging, key
  history (5FC10C), OpenPGP private data objects (0101 to 0104), RSA, GET
  CHALLENGE, MSE.** All answered with a proper status word, none implemented.
- **Extended APDU exchange level.** The CCID layer announces short APDUs. The
  parser already accepts extended length fields, so raising this is a change in
  the descriptor and the chaining logic, not in the applets.

## How it was checked

```bash
cd apps/firmware/test/applets
make                     # 564 test cases
make fuzz FUZZ_SECS=20   # APDU parser and CCID frame
make CC=clang-19         # the same with the second toolchain
```

Both toolchains compile with `-Wall -Wextra -Werror -Wshadow -Wconversion
-Wsign-conversion` and run under AddressSanitizer and UndefinedBehaviorSanitizer.
An extra clang pass with `-Wmissing-prototypes -Wcomma
-Wunreachable-code-aggressive -Wcast-align -Wdocumentation` found nothing.

The fuzz run covers both outer walls — the CCID frame and the APDU parser — and
refuses to pass if it never got a valid command through, so a green run cannot
mean "everything was rejected". It found nothing after roughly two million
commands and twelve million frames per toolchain.

What the tests found while being written, both now fixed: the router wiped the
command-chaining buffer while the applet still pointed into it, and the CRC-16
convention was wrong (the complement is appended, which is what makes the
residue 0xF0B8 hold).

The four levels of "checked" in this project: written / compiled / in the image
/ run on the device. This is at **compiled and run on a PC**. Nothing here is
at the last two levels.
