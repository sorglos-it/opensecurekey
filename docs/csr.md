# Certificate requests: PKCS#10 on the device

How the key asks a certificate authority for a certificate — and why the
request cannot be built anywhere else.

Status: written, compiled and run on a PC with gcc 14, and read back by
OpenSSL 3.5.7, which confirmed the signature. The check counts and the exact
commands are in "How it was checked" below — they are stated once, there, and
not repeated here.
**Nothing has run on the device.** The board is not built and the SE051C2 has
not been delivered, so every run uses a stand-in for the security chip. What
that stand-in assumes about the real chip is listed under "What the stand-in
assumes" — read that section before trusting any of this on hardware.

## Why the request is built on the device

A certificate request is *signed with the private key*. That is its whole
point: it proves to the authority that whoever asks owns the key belonging to
the public part in the request.

On this device the private key is created inside the security chip and never
leaves it. So the request has to be assembled where the signing happens — on
the device. The management program may ask for a request and hand it out; it
cannot build one, because building one needs the key.

This is the same reason the device speaks PKCS#11 to software that wants a key
file: the certificate may leave, the private key never does.

## What is in a request

`CertificationRequestInfo` as RFC 2986 section 4 defines it:

| Field | What this device writes |
| --- | --- |
| `version` | `0`. Not 1 — the number counts from 0 here, unlike X.509, where v3 is the 2. |
| `subject` | CN (required), and optionally C, O, OU and serialNumber, in that order: general to specific. |
| `subjectPKInfo` | The public key as the chip reports it — an EC point for P-256 and P-384, modulus and exponent for RSA. |
| `attributes [0]` | Always present, even when empty. A request without the field is not a request. |

Around that: the signature algorithm and the signature itself.

**The extensions are not a field.** SAN, Key Usage and Extended Key Usage
travel inside `attributes` as the `extensionRequest` attribute, 1.2.840.113549
.1.9.14 (RFC 2985 section 5.4.2). That is four levels of nesting, three of
them sets. Anyone looking for an extensions field will not find one.

Supported extensions:

* **subjectAltName** — up to four names, each a DNS name, an e-mail address or
  a URI. Not marked critical, because the subject is not empty (RFC 5280
  section 4.2.1.6 only requires the marking when it is).
* **keyUsage** — marked critical, because RFC 5280 section 4.2.1.3 requires
  it. Without the marking a verifier is allowed to ignore the field, and then
  it says nothing.
* **extendedKeyUsage** — server auth, client auth, code signing, e-mail
  protection. Not critical, and therefore written *without* an explicit
  `FALSE`: DER leaves a default value out, and an explicit `FALSE` would be
  BER.

## Key types and the signature

| Key | Public key OID | Signature algorithm |
| --- | --- | --- |
| P-256, P-384 | id-ecPublicKey + the curve | ecdsa-with-SHA256, no parameter |
| RSA 2048 / 3072 / 4096 | rsaEncryption + an explicit NULL | sha256WithRSAEncryption + an explicit NULL |

Two details that look like nitpicking and are not:

* The RSA algorithm identifier carries an **explicit NULL** parameter, the
  ECDSA one carries **nothing at all**. RFC 8017 appendix A.2.3 against
  RFC 5758 section 3.2 — not the same thing, and some verifiers refuse the
  key if the NULL is missing.
* A modulus with its top bit set needs a **leading zero byte** in the DER
  INTEGER, or it is a negative number. This is the mistake that broke the RSA
  attestation certificate on 2026-10-29. A curve point never hits it, because
  nothing there is longer than 127 bytes.

**Signing always uses SHA-256**, including for a P-384 key. That is the only
hash this layer has, and the hash is computed on the host side of the contract
rather than in the chip, so the display can later show exactly what is being
signed. The algorithm identifier states precisely what was done; nothing is
mixed silently.

RSA signing uses PKCS#1 v1.5 padding, named explicitly on every call, as the
chip contract requires. PSS is not built — see "Open points".

## Output

`osk_csr_der()` gives DER, `osk_csr_pem()` the same bytes as PEM (base64 in
64-character lines between the two delimiter lines of RFC 7468 section 4).

PEM is deliberately a separate call over the finished DER, not a second build.
Building twice would mean signing twice, and two signatures over two slightly
different requests for one key is exactly the kind of thing nobody notices
until an authority rejects one of them.

## Where the code lives

| File | What is in it |
| --- | --- |
| `apps/firmware/src/classes/applets/osk_csr.c` | The request itself: subject, attributes, signing, PEM. |
| `apps/firmware/src/classes/applets/osk_der.c` | Writing DER: tags, object identifiers, INTEGER sign handling, BIT STRING, string type choice, extensions. |
| `apps/firmware/src/classes/applets/osk_spki.c` | `SubjectPublicKeyInfo` from a chip key. |

`osk_der.c` and `osk_spki.c` were split out of `osk_attest.c` on 2026-10-30.
The attestation certificate and the certificate request need byte-identical
output for the same key, and the same number written down twice goes stale in
one of the two places. The attestation output did not change: its 189 checks
pass unchanged before and after the split.

### What chooses nothing

`osk_csr.c` does not invent a subject, does not pick extensions, and **does
not ask for a PIN or a button press**. The caller does all of that, because
the caller knows the slot policy.

Unlike attestation (`osk_attest.h`), that is not harmless here: attestation
signs with a separate key over data it assembles itself, whereas a certificate
request is signed with *the user's own key*. Calling this without the slot's
release rule satisfied is a bug in the caller.

## What the stand-in assumes

The chip is not here. The stand-in (`apps/firmware/test/applets/mocks.c`) was
written against the wording of `apps/firmware/include/osk_se050.h`, not against
what seemed likely. These assumptions are open until a real SE051C2 answers:

1. **`osk_se050_sign()` returns the ECDSA signature as DER** — a SEQUENCE of
   two INTEGERs — and that is what goes into the BIT STRING. The contract does
   not say so in words; the card layer's `osk_card_sig_der_to_raw()` implies
   it, since it exists to convert that form into the flat one. If the chip
   returns `r || s` instead, every ECDSA request is malformed and openssl will
   say so immediately.
2. **`osk_se050_key_public()` wraps the point in something.** The stand-in
   puts two bytes in front of it; the real chip's wrapper may be longer or
   shaped differently. `osk_card_public_point()` searches for the point rather
   than assuming an offset, which is why this is a small risk rather than a
   large one.
3. **The public exponent comes back three bytes long** (`01 00 01`). The
   contract warns explicitly against assuming four. Both lengths are handled;
   only the three-byte case has actually run.
4. **`osk_se050_rsa_sign()` returns a signature exactly as long as the
   modulus** and does the PKCS#1 v1.5 padding itself when told
   `OSK_RSA_PAD_PKCS1_V15`. If the chip expects a pre-padded block instead,
   the signature will be over the wrong bytes — and that is the failure mode
   that looks green on a PC.
5. **A key that exists can be read and signed with** without any further
   state. No session, no explicit select.

Point 4 is the one that bit this project before, in a different place: on
2026-10-29 a hand-written stand-in kept a counter differently from the real
counterpart. Everything was green and the device would not have signed at all.
A stand-in checks our side of the contract, never the other side.

## How it was checked

Three things were run, and they prove different things.

**1. The applet check set**, structure and behaviour, against the stand-in:

```powershell
cd apps\firmware\test\applets
.\run.ps1 -Both
```

| Build | Cases | Failures |
| --- | --- | --- |
| with RSA (`CONFIG_OSK_RSA=1`) | 1818 | 0 |
| without RSA | 1542 | 0 |

Of those, `test_csr` contributes 434 cases with RSA and 320 without. It takes
the request apart with its own DER reader — checking against X.690, not against
the code under test — and checks, among other things, that the signed part of
the finished request is byte-identical to what `osk_csr_info()` produces, and
that the hash handed to the chip is SHA-256 over exactly those bytes.

Both directions of the Kconfig option are built and run. Without RSA an RSA
request is refused with `OSK_ERR_UNSUPPORTED` rather than built with an empty
key, and the same answer is checked for the other reason it can happen: an
SE051A2 in the socket, which cannot do RSA at all.

**2. The proof with a foreign program.** This is the one that matters: a
request only our own reader understands is worth nothing.

```powershell
cd apps\firmware\test\applets
.\run.ps1 -Openssl -Both
```

OpenSSL generates the key pair, the firmware builds the signed part, OpenSSL
signs exactly those bytes, the firmware assembles the request, and then
`openssl req -verify -text` reads it back. Result with OpenSSL 3.5.7:

| Case | Result |
| --- | --- |
| p256 | read, `Certificate request self-signature verify OK`, ecdsa-with-SHA256 |
| p384 | read, verify OK, ecdsa-with-SHA256 |
| rsa2048 | read, verify OK, sha256WithRSAEncryption |
| rsa3072 | read, verify OK, sha256WithRSAEncryption |
| rsa4096 | read, verify OK, sha256WithRSAEncryption |

Both the DER and the PEM output are verified, and OpenSSL's own `-text` output
is searched for every piece the firmware built: the full subject in the right
order, all four alternative names with their correct types, the critical key
usage with both bits, and both extended key usages. In the build without RSA
the three RSA cases are reported as skipped — a skipped proof must not read
like a delivered one.

**What this does not prove:** that the SE051C2 produces that signature. Here
OpenSSL signs in its place. See "What the stand-in assumes".

**3. The repository check**, `python tools/check.py`, which runs the check set
above among everything else.

Stage reached: **2 of 4** — written, compiled, run on a PC. Not in an image,
not on the device.

## Open points

* **No command asks for a request yet.** The module is there; nothing calls
  it. A card command along the lines of attestation's `INS F9`, or a path in
  the management program, is a separate piece of work.
* **PSS is not built.** An RSASSA-PSS algorithm identifier carries its own
  parameters (hash, mask function, salt length), and getting those wrong
  produces a request that verifies nowhere. PIV uses v1.5 (SP 800-73-4 part 2),
  so v1.5 is what is built. OpenPGP allows both.
* **No challengePassword attribute.** Some authorities ask for one. The
  attribute slot is there; nothing fills it.
* **What a CA will need from this** — and is deliberately *not* built here,
  because a CA without revocation is not being built at all (`docs/plan.md`):
  a stable way to map a request to the slot it came from, and the attestation
  certificate alongside the request, so the authority can see that the key
  really is in hardware. Both exist already; nothing ties them together yet.
* **No test against a real CA.** Nothing has been sent to one, and no issued
  certificate has been loaded back into a slot.
