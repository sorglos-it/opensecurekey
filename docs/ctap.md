# FIDO2 / CTAP

What the key does when a website or a program asks it to log you in.

Status: written and compiled on a PC, tests and a fuzz run green.
**Nothing has run on the device, and nothing has run in a browser** — the
board is not built yet.

## Which specification

| Specification | Version used |
| --- | --- |
| Client to Authenticator Protocol | CTAP 2.2, Proposed Standard, 14 July 2025 |
| Web Authentication | WebAuthn Level 3 (W3C, authenticator data and packed attestation) |
| CTAP1 / U2F | FIDO U2F Raw Message Formats v1.2, and CTAP 2.2 section 10 |
| HKDF | RFC 5869 · HMAC RFC 2104 · SHA-256 FIPS 180-4 · AES FIPS 197 |

`authenticatorGetInfo` reports `versions` as `U2F_V2`, `FIDO_2_0`, `FIDO_2_1`.
It does not claim `FIDO_2_2`, because the 2.2-only commands
(`authenticatorConfig`, `authenticatorCredentialManagement`,
`authenticatorBioEnrollment`, `authenticatorLargeBlobs`) are not implemented.

## Commands

| Command | Code | State |
| --- | --- | --- |
| authenticatorMakeCredential | 0x01 | done |
| authenticatorGetAssertion | 0x02 | done |
| authenticatorGetInfo | 0x04 | done |
| authenticatorClientPIN | 0x06 | done, protocols 1 and 2 |
| authenticatorReset | 0x07 | done, blocked over NFC |
| authenticatorGetNextAssertion | 0x08 | done, 30 s window |
| authenticatorSelection | 0x0B | done |
| authenticatorBioEnrollment | 0x09 | not implemented |
| authenticatorCredentialManagement | 0x0A | not implemented |
| authenticatorLargeBlobs | 0x0C | not implemented (the largeBlob extension is) |
| authenticatorConfig | 0x0D | not implemented |

CTAP1/U2F: `U2F_REGISTER`, `U2F_AUTHENTICATE` (all three control bytes) and
`U2F_VERSION`.

## Extensions

| Extension | State |
| --- | --- |
| `hmac-secret` | done — this is what unlocks Bitwarden, BitLocker and LUKS |
| PRF (WebAuthn) | done by way of `hmac-secret`, see below |
| `largeBlob` | done, the extension from section 12.4 (blob travels in GetAssertion) |
| `credProtect` | done, levels 1 to 3, the level travels inside the credential ID |
| `minPinLength` | done, only for relying parties on the authorised list |
| `credBlob`, `largeBlobKey`, `hmac-secret-mc`, `thirdPartyPayment` | not implemented |

### PRF has nothing extra to do on the device

WebAuthn's `prf` extension is a *client* extension. The browser turns each
input into a salt — `SHA-256("WebAuthn PRF" || 0x00 || input)` — and sends it
to the key as `hmac-secret`. So a key that gets `hmac-secret` right supports
PRF, and there is no `prf` key in any CTAP message. WebAuthn Level 3 also
requires that the PRF use the secret for the *user-verified* case, which is
the platform's job to arrange by asking for user verification.

## Keys are derived, never stored

There is no key per service. One root secret lives in the SE050 and never
leaves it. For every credential the chip derives:

| Purpose | Derivation input |
| --- | --- |
| signing key | `"osk/fido/key/v1"` ‖ salt ‖ rpIdHash |
| CredRandomWithUV | `"osk/fido/hmac-uv/v1"` ‖ salt ‖ rpIdHash |
| CredRandomWithoutUV | `"osk/fido/hmac-nouv/v1"` ‖ salt ‖ rpIdHash |
| largeBlobKey | `"osk/fido/largeblob/v1"` ‖ salt ‖ rpIdHash |
| credential-ID wrapping key | `"osk/fido/credid/v1"` |

Each purpose has its own label, so no two derivations can ever produce the
same value. Section 12.3 requires exactly that for the largeBlobKey: it must
not be reachable through `hmac-secret` with any salt.

The number of services is therefore unlimited **in the "mirrored" mode**: the
chip's user memory holds one secret, not one key per site. A paired second key
that holds the same root secret is a real spare: it derives the same keys.

**In the "this device only" mode the number is finite** — there each credential
is a permanent key pair inside the chip and occupies its memory. Which mode a
profile uses, and the size of that memory, are stated in one place only:
`docs/architecture.md`, section "Two key modes, chosen per profile". Do not
repeat a capacity number here.

### The credential ID carries everything needed

97 bytes:

```
[0]       version (0x01)
[1..16]   random, also the AES-CBC IV
[17..80]  AES-256-CBC over 64 bytes of content
[81..96]  HMAC-SHA-256 over everything before it, truncated to 16 bytes
```

Content: flags (discoverable, hmac-secret), credProtect level, rpIdHash, the
16-byte derivation salt, and the creation time.

The MAC is checked **before** anything is decrypted, and the comparison is
time-independent. A credential ID from another device, or one with a single
bit changed, is rejected as "not found" and nothing more is revealed. A test
flips every single byte in turn and checks that none of the 97 variants is
accepted. After `authenticatorReset` the root secret is new, so every old
credential ID is worthless — which is exactly what a reset should mean.

Discoverable credentials (resident keys) additionally need the user handle, so
they get a record in the vault (`osk_store.h`); the derived key still comes
from the chip.

## PIN

Protocol 2 is offered first and is the one we want: separate keys for
encryption and MAC, AES-256-CBC with a fresh random IV, full-length
HMAC-SHA-256. Protocol 1 stays available because older software knows only
that one.

- The PIN never reaches this code as a PIN: only `LEFT(SHA-256(PIN), 16)`.
- The chip checks it, and **the retry counter lives in the chip** — wiping the
  flash does not reset it.
- Three consecutive wrong PINs return `CTAP2_ERR_PIN_AUTH_BLOCKED`: the key has
  to be unplugged before it will try again.
- Minimum length is 4 Unicode code points (counted as code points, so four
  umlauts are four characters, not eight).

Built-in user verification is the fingerprint sensor, through
`osk_ui_verify_user()`; `getPinUvAuthTokenUsingUvWithPermissions` uses it.

## Every release is shown on the display

Every operation that signs or reveals anything goes through
`osk_ui_confirm()`, and the **service name is passed as the subject**, so the
display names what is being approved. Malware on the computer can ask, but it
cannot press the button, and it cannot change what the display says.

## Transport: USB-HID

Framing, channels, `INIT`, `PING`, `KEEPALIVE`, chaining and timeouts, per
section 11.2. One assembly buffer of 1300 bytes, a table of four channels,
nothing allocated at runtime. A message that announces more than the buffer
holds is rejected whole (`ERR_INVALID_LEN`), never read in part. An
incomplete message expires after 500 ms. `CTAPHID_LOCK` is not implemented
(the specification makes it optional) and answers `ERR_INVALID_CMD`.

`maxMsgSize` is 1200 bytes, `maxCredentialCountInList` 8,
`maxCredentialIdLength` 97.

## The parser is treated as the dangerous part

- Every length is checked **before** any byte is read.
- No dynamic memory anywhere, fixed upper bounds throughout.
- Only CTAP2 canonical CBOR is accepted: smallest encoding, definite lengths,
  sorted and unique map keys, no tags, no floats, at most 4 levels of nesting.
  Trailing bytes are an error.
- Each message is validated as a whole before it is interpreted.
- The writer also produces only canonical CBOR, and the tests read every
  response back with the strict reader — the writer may never emit something
  our own parser would reject. The fuzz run asserts the same thing.

## What was tested, and how

Run on a PC (`apps/firmware/test/ctap`, `make` and `make fuzz`), compiled with
`-Wall -Wextra -Werror -Wconversion` and with AddressSanitizer and
UndefinedBehaviorSanitizer on.

| Set | Cases |
| --- | --- |
| `test_crypto` | 35 — SHA-256, HMAC (RFC 4231), HKDF (RFC 5869), AES-256-CBC (FIPS 197, SP 800-38A) against the published vectors |
| `test_cbor` | 88 — canonical form accepted, everything else rejected |
| `test_credid` | 33 — including: every byte flipped, none accepted |
| `test_pin` | 173 — protocols 1 and 2 played from the computer's side |
| `test_ctap` | 226 — real messages in, responses checked field by field, including a count proving that looking up a passkey opens only the one entry that matches |
| `test_u2f` | 34 — framing, register, authenticate, a U2F handle used over CTAP2 |
| `test_hid` | 148 — channels, chaining, sequence errors, busy, timeout |
| `fuzz_cbor` | 5.3 million rounds, 93 000 of them accepted messages |
| `fuzz_hid` | 50 million packets in, 1.2 million CBOR answers out |

The fuzz runs check themselves: if no message ever gets through, they report a
failure instead of a green run, because then they would only have tested the
outermost check.

### What the tests do NOT prove

- **No device, no browser.** Nothing here has talked to a real computer, a
  real website, or a real SE050.
- **The SE050 is a stand-in.** The test double is deterministic but does not
  compute real P-256: public points, ECDH and signatures are hashes of the
  right shape and length. So the message *structure* is tested, not the curve
  arithmetic. Real ECDSA/ECDH is the SE050 strand's job.
- **No conformance test.** The FIDO test tools have not been run.
- **No audit.**

## Known gaps, deliberate and otherwise

- **Attestation is self attestation** (`packed`, no `x5c`). That is the most
  privacy-friendly option and needs no certificate in the device, but some
  enterprise relying parties want a real attestation certificate. The U2F
  registration response therefore carries no attestation certificate either,
  which some old U2F-only sites may reject.
- **The signature counter lives in RAM** and starts at zero on every power-up.
  WebAuthn permits a constant zero counter, but a counter that goes backwards
  looks like a cloned key to a relying party. It needs a home in flash.
- **AAGUID is provisional** and gets its final value with the first batch.
- **Backup flags (BE/BS) are zero.** Mirroring to a second key is off by
  default; when it is switched on, these flags have to follow.
- **A discoverable credential still has to be opened to be used.** Finding it
  does not: `osk_store_find_tag()` searches by a tag (the rpIdHash for a
  passkey, a hash of the derivation salt for a large blob), so only the entry
  that actually matches is decrypted. A test counts the openings and fails if
  there is more than one. The user handle itself lives in the encrypted
  record, so that one opening remains.

## Where fingerprints get enrolled — and why not in the browser

`authenticatorBioEnrollment` (0x09) is **not implemented**, and that is a
decision, not a gap. `osk_pin.c` refuses `OSK_PERM_BE` explicitly. No browser
and no website can enrol a finger on this key; enrolment happens on the device
itself, through the display and the button.

The reason is the same one the display exists for: enrolling a finger from the
host means trusting the host about which finger is being added. On a machine
with malware that is the wrong place to decide it.

What the host *does* get, once a sensor is fitted: the finger counts as user
verification through `osk_ui_confirm()`, and `getInfo` reports `uv`. No change
to the CTAP core is needed for that — it was built that way.

**Until a sensor is fitted, `getInfo` must not report `uv` at all.** In CTAP 2.1
that option means built-in user verification — a fingerprint or the like — not
the PIN, which is reported separately through `clientPin`. Reporting `uv: true`
with only a PIN tells a relying party the device can verify a user by itself,
which it cannot. It stood that way until 24.10.2026, found by the fingerprint
search while checking whether a sensor would need CTAP changes. A test now
fails if the option comes back (`test_ctap.c`, "uv wird NICHT gemeldet").
