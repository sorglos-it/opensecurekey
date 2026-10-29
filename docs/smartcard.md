# Smart card side: PIV, OpenPGP, one-time passwords

What the key does when a computer treats it as a chip card.

Status: written, compiled and run on a PC with gcc 14 and clang 19, fuzz runs
clean under AddressSanitizer and UndefinedBehaviorSanitizer. The check counts
and the exact command are in "How it was checked" below — they are stated once,
there, and not repeated here.
**Nothing has run on the device, and nothing has run against a real smart card
stack** — not pcscd, not Windows, not macOS, not GnuPG, not OpenSC, not
KeePassXC, not VeraCrypt. The board is not built yet, so every test runs
against stand-ins for the SE050, the vault and the display.

This is level **2 of four**: written / compiled / in the image / run on the
device.

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
| SHA-1 · SHA-256 · HMAC · AES-128 | FIPS 180-4 · RFC 2104 · FIPS 197 |
| RSA padding, DigestInfo | RFC 8017 (PKCS #1 v2.2), section 9.2 |
| X.509 certificates (attestation) | RFC 5280; RFC 5480 for the curve OIDs |
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
| GENERATE ASYMMETRIC KEY PAIR | done, P-256, P-384 and **RSA-2048**, plus slot **F9** |
| GENERAL AUTHENTICATE | done, signing (tag 81), key agreement (tag 85, slot 9D only) and **RSA decryption** (tag 81 in slot 9D) |
| ATTEST (INS F9) | done, one certificate per slot |

Keys live in the SE050 and never leave it; certificates live in the vault as
`OSK_ITEM_CERT`. Slot 9C enforces "PIN always": the PIN must be verified in the
command directly before the signature, and a second signature without a fresh
VERIFY is refused. Slot 9E needs no PIN, as the specification says — but it
still needs the button.

**Only RSA-2048 under PIV, and that is a citation, not a shortcut.**
SP 800-73-4 part 1 table 6-2 gives algorithm identifier `07` for "RSA 2048 bit
modulus, 65537 exponent" and gives none for 3072 or 4096. Those appear in
SP 800-73-5, which is not available here, so their identifiers are recorded as
**open** rather than guessed: a wrong identifier produces a certificate nobody
can verify, and the error shows up at the other end. OpenPGP is not affected —
it carries the modulus length as a number in the data object, so it offers all
three lengths the chip can do.

**Tag 81 means two different things, and the slot decides which.** In 9A, 9C
and 9E it is a padded signature block; in 9D it is a ciphertext. With elliptic
curves the question does not arise, because key agreement has a tag of its own
(85). This is the project's recurring failure — two quantities with the same
name measuring different things — and it is written down at the call site as
well.

**RSA signing deliberately narrows what PIV allows.** PIV leaves the padding to
the caller and expects the card to perform the **raw** private-key operation
(part 2, section 3.2.4). That would be one chip command — raw decryption and
raw signing are the same arithmetic. It is not done, and the reason is slot 9E:
that slot needs no PIN, and a raw private-key operation on a value the computer
chooses freely is a signing oracle for anything. Instead the block is
**unwrapped**: the padding is checked, the DigestInfo is recognised, the hash
and the hash algorithm are taken out of it, and the signature then goes through
the contract's named, padded path. The result is byte-identical for a
well-formed v1.5 block carrying SHA-256, SHA-384 or SHA-512. Anything else —
SHA-1, PSS, a hand-made block — is refused with `6A80`. What that costs is in
"What is missing".

**The hash algorithm is never derived from the length.** SHA-512/256 and
SHA-256 are both 32 bytes, so the length cannot tell them apart; the algorithm
comes out of the DigestInfo OID. A test feeds a SHA-512/256 DigestInfo, which
is exactly as long as a SHA-256 one, and requires that it be refused rather
than treated as SHA-256.

### Attestation per slot (`src/classes/applets/osk_attest.c`)

What it proves: that the key in a slot was created inside the security chip and
has never left it. Without it, "the key is in hardware" is a claim — a public
key looks the same either way.

How: a second key in the chip, slot **F9**, signs an X.509 certificate over the
slot key's public part. The command is `INS F9`, `P1` = slot, no data; the
answer is the certificate as DER. The F9 certificate itself lives in data
object `5FFF01` and is written from outside (`PUT DATA`); it is only read here,
to take the issuer name from it.

- **The issuer is copied byte for byte** out of the stored F9 certificate. The
  issuer of one certificate has to be byte-identical to the subject of the
  next, or no verifier finds the chain; a rebuilt name with the same text in a
  different string type is a different name. With no F9 certificate stored, a
  fallback name is used that says `unchained` — the chain is then not closed,
  and that should be visible.
- **Validity is fixed**, 2000-01-01 to 9999-12-31. The device has no clock it
  may trust — the time comes from the computer on insertion — and a certificate
  dated from a set clock would be valid sometimes and not others.
  `99991231235959Z` is the form RFC 5280 section 4.1.2.5 provides for "no
  well-defined expiration date".
- **No invented object identifiers.** The slot, the device serial and the slot's
  release policy go into standard subject fields (CN, `serialNumber` 2.5.4.5,
  OU). An identifier nobody assigned is somebody else's one day. The extensions
  are `keyUsage` and `basicConstraints`, both standard, both marked critical.
- **No PIN and no button**, and the reason matters: nothing secret is released —
  a public key and a name — and the F9 key signs only data this module assembles
  itself, never anything arriving from outside. It is therefore not a signing
  oracle. Were that different, approval would be mandatory.
- **The certificate's serial number depends on the public key**, not only on
  device and slot. Regenerating the key in the same slot would otherwise give a
  second certificate with the same serial from the same issuer, which RFC 5280
  section 4.1.2.2 forbids.

**F9 must be a P-256.** The signature runs over SHA-256, the only hash this
layer has. Signing with a P-384 key would be allowed but would make the
algorithm identifier in the certificate differ from the one written there, so it
is refused instead of quietly mixed.

**The numbers F9 and 5FFF01** come from the brief and sit in the ranges
ISO/IEC 7816-4 leaves for private use. They happen to match what is common on
the market; a number in an interface is not code, and nothing was taken from
anywhere. **The certificate's content is our own**, so foreign tools will not
find the statements where they look for them. Compatibility with a particular
tool would need that tool's description, which is not available here.

### Static password (`src/classes/applets/osk_static_pw.c`)

The card side only: two slots, each holding a fixed string in the vault, read
through `osk_static_pw_fetch()`. The **typing** belongs to the USB keyboard
transport, and the seam deliberately points that way — this module calls
nothing in the transport, so it needs no header from there and the two can be
built side by side. What the transport has to offer is one line:

```c
osk_result_t osk_kbd_type(const uint8_t *ascii, size_t len, uint8_t layout, bool enter);
```

and it owns the numbering of the keyboard layouts. Until it names any, the card
accepts only `OSK_KBD_LAYOUT_US` (0) — better one layout that is demonstrably
meant than a number that means something different on each side.

**Printable ASCII only, 0x20 to 0x7E.** Not for convenience: every character
becomes a key code when typed, and where that code sits depends on the
computer's layout. A character above 0x7F arrives as a different character on a
foreign layout, and a password that sometimes arrives differently is worse than
none. What is refused is refused when the password is **set**, not when it is
typed — and again when it is read back, because the record comes out of flash
and this build did not necessarily write it.

Fetching opens a vault record and therefore asks on the display. That is not
optional: this is the one place where a secret leaves the device as typed text
that any program can read along.

### One serial number per device (`src/classes/applets/osk_serial.c`)

Derived in the security chip from the root secret, through
`osk_se050_hkdf()` with its own purpose string. Not from the nRF52840's FICR:
that chip's read protection has been defeated with voltage glitches, and a
value anyone can copy off the outside is no better as an identifier while
costing a second source. Not from a production step, because there is none.

It is unique per device because the root secret is, stable across restarts
because the derivation is, and it **changes on a factory reset** — which is
intended: afterwards it is a different device to every service.

**It is not a proof.** The serial is public: it sits in the card's application
identifier and in every attestation certificate. Anybody can claim any number.
What cannot be forged is the F9 key's signature over it. If the chip does not
answer, there is **no** serial rather than a made-up one — a stand-in value
would be the same on every device, which is the gap this closes.

That gap was real: GnuPG tells cards apart by the serial in the application
identifier, and until 29 October 2026 every device carried the same
`00 53 4B 01`, so two of our sticks on one computer looked like one card.

### OpenPGP (`src/classes/applets/osk_openpgp.c`)

| Command | State |
| --- | --- |
| SELECT | done (returns no data, per section 7.2.1) |
| VERIFY (PW1 81/82, PW3 83) | done, including query and P1 = 0xFF |
| CHANGE REFERENCE DATA (PW1, PW3) | done |
| RESET RETRY COUNTER | done, with PW3 (P1 = 02) and with the resetting code (P1 = 00) |
| GET DATA | 4F, 5E, 5F50, 5F52, 65, 6E, 7A, C0, C4 |
| PUT DATA | 5B, 5F2D, 5F35, 5E, 5F50, C1 to C4, C7 to C9, CA to CC, CE, D3 |
| GENERATE ASYMMETRIC KEY PAIR | done, generate (P1 = 80) and read (P1 = 81), curves and **RSA** |
| PSO: COMPUTE DIGITAL SIGNATURE | done; curves give flat r ‖ s per RFC 6637, **RSA gives a signature as long as the modulus** |
| PSO: DECIPHER | done, ECDH from A6 → 7F49 → 86, and **RSA from `00` ‖ ciphertext** |
| INTERNAL AUTHENTICATE | done, curves and **RSA** |
| TERMINATE DF / ACTIVATE FILE | done, wipes keys and card data |

PW1 is valid for one signature unless the PW status byte says otherwise, and
the signature counter in DO 93 counts along, for RSA as well.

**RSA in all three lengths**, because the algorithm attributes carry the modulus
length as a number (section 4.4.3.9): `01` ‖ modulus bits ‖ exponent bits ‖
import format. Which lengths actually work is answered by the chip, not by this
file. The **exponent length reported back is the chip's**, not the one a card
program asked for — the contract warns that it is three or four bytes per chip
and per key and that assuming 65537 builds a wrong certificate eventually. The
stand-in returns three bytes on purpose, so code that assumes four fails the
test.

**OpenPGP sends a DigestInfo, not a bare hash** (section 7.2.10: the card pads,
the computer builds the DigestInfo). The contract wants the two apart, so the
DigestInfo is parsed and the hash algorithm comes out of its OID. SHA-1 and
RIPEMD-160 are refused with `6A80`; that is a narrowing against the
specification and it is in "What is missing".

**Decryption is PKCS#1 v1.5 only.** OAEP is not available on this chip, and
that is documented rather than unfinished: AN12413 table 32 knows OAEP only with
SHA-1, and `osk_hash_t` has no SHA-1 (see `docs/se050.md`). OpenPGP encrypts
session keys with v1.5, so it costs nothing here.

A key slot holds exactly **one** key — a curve or an RSA key, never both. The
vault record that survives a power cut grew the two RSA fields and its version
went from 1 to 2; version 1 is still accepted when reading and means "no RSA
key", so a record from an older build does not send the card back to its
defaults. Card holder data, fingerprints,
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

**Curves and RSA.** P-256, P-384 and RSA-2048 for PIV; P-256, P-384 and RSA in
2048, 3072 and 4096 for OpenPGP. **Two questions have to say yes**, and they are
not the same question: whether RSA was *built* (the Kconfig option
`CONFIG_OSK_RSA`) and whether the delivered chip *can* do it
(`osk_se050_rsa_sizes()`). The SE051**A2** cannot do RSA at all, the **C2** can,
and both answer on the same I²C address — so the firmware asks at startup and
reports it, instead of failing on the first key. On a part without RSA both
applications keep working without RSA: the algorithm is not even offered in
PIV's SELECT list, and RSA commands answer "function not supported".

The option has exactly **one** place where it takes effect, `osk_rsa_available()`.
The rest of the RSA code is compiled into both variants on purpose: a path only
one variant ever shows the compiler is not checked in the other. What the
no-RSA variant really saves is in the driver (`src/classes/se050/`), not here.

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
- **PIV algorithm identifiers for RSA-3072 and RSA-4096.** SP 800-73-4 names
  none; SP 800-73-5 does, and it is not available here. Recorded as open, not
  guessed. OpenPGP offers all three lengths regardless.
- **RSA with SHA-1, with PSS, or with a hand-made padding block** is refused on
  both applications. The chip can sign PSS, but `osk_hash_t` has no SHA-1 and
  PIV's raw path is deliberately closed (see above). A card program that insists
  on one of these gets `6A80`.
- **RSA-OAEP decryption.** Not a gap in this folder: the chip offers OAEP only
  with SHA-1 (`docs/se050.md`). PIV slot 9D and the OpenPGP encryption key both
  use v1.5, so nothing is lost today.
- **The attestation certificate is not compatible with any vendor's tool.** The
  structure is X.509 and valid, the statements inside it are ours. That is a
  decision for whoever wants a particular tool to read it.
- **Whether `openssl x509` accepts the certificate has not been measured.** The
  tests take it apart field by field against RFC 5280, which is not the same as
  a second implementation parsing it.
- **PIV management key (9B), biometrics, card-level secure messaging, key
  history (5FC10C), OpenPGP private data objects (0101 to 0104), GET
  CHALLENGE, MSE.** All answered with a proper status word, none implemented.
- **Extended APDU exchange level.** The CCID layer announces short APDUs. The
  parser already accepts extended length fields, so raising this is a change in
  the descriptor and the chaining logic, not in the applets.

## How it was checked

```bash
cd apps/firmware/test/applets
make                     # the variant WITH RSA:    1130 checks
make RSA=0               # the variant WITHOUT RSA:  968 checks
make fuzz FUZZ_SECS=20   # APDU parser and CCID frame
make CC=clang-19         # all of it again with the second toolchain
```

On Windows there is neither `make` nor `gcc`, and the test harness
(`test/ctap/test_util.h`) uses GCC extensions that `cl` does not have, so
`run.ps1` next to the Makefile calls **that same Makefile** inside WSL:

```powershell
.un.ps1 -Both -Fuzz
```

`run.ps1` builds nothing itself. That is deliberate: on 28 October 2026 the
SE050 test run had **two** build descriptions, a Makefile and a `build.ps1`,
and the `build.ps1` did not list one source file. The Windows path therefore
never linked, and it looked like a caller error rather than a missing source.
Two descriptions of one test drift apart; one cannot.

`python tools/check.py` passes, but it reports `firmware/tests` as **skipped**
(no cmake, no make on Windows) — so it does **not** run any of the checks
above. "check.py is green" is not a statement about this folder.

**Both variants must be built and run.** An option tested in one direction only
is not an option. The counts differ because the RSA checks are replaced by their
opposite expectations in the no-RSA variant, not skipped.

Both toolchains compile with `-Wall -Wextra -Werror -Wshadow -Wconversion
-Wsign-conversion` and run under AddressSanitizer and UndefinedBehaviorSanitizer.
An extra clang pass with `-Wmissing-prototypes -Wcomma
-Wunreachable-code-aggressive -Wcast-align -Wdocumentation` found nothing.

The fuzz run covers both outer walls — the CCID frame and the APDU parser — and
refuses to pass if it never got a valid command through, so a green run cannot
mean "everything was rejected". It found nothing after roughly two million
commands and twelve million frames per toolchain.

What the tests found while being written, all now fixed: the router wiped the
command-chaining buffer while the applet still pointed into it; the CRC-16
convention was wrong (the complement is appended, which is what makes the
residue 0xF0B8 hold); and the DER integer writer in the attestation module wrote
its length field as a single byte, which is correct for every curve — nothing
there exceeds 127 bytes — and breaks for a 257-byte RSA modulus. That last one
only appeared once an RSA key was attested, which is the argument for testing
both variants and both key types rather than one of each.

One expectation in `test_rsa_card.c` was wrong rather than the code: it assumed
`osk_rsa_modulus_bits()` would report 0 in the no-RSA variant. It asks the chip,
not the option, so it answers in both variants; the gate sits one level up, in
the applications, where it is checked in both. The test now states what is true
and says why the gate is only in one place.

The four levels of "checked" in this project: written / compiled / in the image
/ run on the device. This is at **compiled and run on a PC**. Nothing here is
at the last two levels.
